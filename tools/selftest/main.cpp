// Copyright (c) 2026-present cyan-cs
//
// Licensed under the MIT License.
// https://opensource.org/license/mit
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.



#include "ime/dictionary/dictionary.hpp"
#include "ime/engine/ime_engine.hpp"
#include "ime/input/key_event.hpp"
#include "ime/state/ime_states.hpp"
#include "platform/tsf/module_state.hpp"
#include "platform/tsf/text_service.hpp"
#include "utils/app_paths.hpp"
#include "utils/com_ptr.hpp"
#include "utils/logger.hpp"

#include <msctf.h>
#include <objbase.h>
#include <wrl/client.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {

using yume::ime::engine::ImeEngine;
using yume::ime::dictionary::Dictionary;
using yume::ime::dictionary::DictionaryStoragePaths;
using yume::ime::input::KeyCode;
using yume::ime::input::KeyEvent;
using yume::ime::state::ImeState;

namespace tsf = yume::platform::tsf;

} // namespace

namespace yume::platform::tsf {

class TextServiceRegistrationTestAccessor {
public:
    using ContextSinkRegistration = TextService::ContextSinkRegistration;
    using UiElementRegistration = TextService::UiElementRegistration;
};

} // namespace yume::platform::tsf

namespace {

class FakeSource final
    : public yume::utils::ComObjectBase<FakeSource>
    , public ITfSource {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == IID_IUnknown || riid == IID_ITfSource) {
            return yume::utils::ComObjectBase<FakeSource>::queryInterfacePointer(
                ppvObject,
                static_cast<ITfSource*>(this));
        }
        return yume::utils::ComObjectBase<FakeSource>::queryInterfacePointer(ppvObject, nullptr);
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return yume::utils::ComObjectBase<FakeSource>::AddRef();
    }

    ULONG STDMETHODCALLTYPE Release() override {
        return yume::utils::ComObjectBase<FakeSource>::Release();
    }

    HRESULT STDMETHODCALLTYPE AdviseSink(REFIID, IUnknown*, DWORD*) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE UnadviseSink(DWORD cookie) override {
        ++unadviseCount;
        lastCookie = cookie;
        return S_OK;
    }

    int unadviseCount = 0;
    DWORD lastCookie = TF_INVALID_COOKIE;

private:
    friend class yume::utils::ComObjectBase<FakeSource>;
    ~FakeSource() = default;
};

class FakeUiElementMgr final
    : public yume::utils::ComObjectBase<FakeUiElementMgr>
    , public ITfUIElementMgr {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == IID_IUnknown || riid == IID_ITfUIElementMgr) {
            return yume::utils::ComObjectBase<FakeUiElementMgr>::queryInterfacePointer(
                ppvObject,
                static_cast<ITfUIElementMgr*>(this));
        }
        return yume::utils::ComObjectBase<FakeUiElementMgr>::queryInterfacePointer(ppvObject, nullptr);
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return yume::utils::ComObjectBase<FakeUiElementMgr>::AddRef();
    }

    ULONG STDMETHODCALLTYPE Release() override {
        return yume::utils::ComObjectBase<FakeUiElementMgr>::Release();
    }

    HRESULT STDMETHODCALLTYPE BeginUIElement(ITfUIElement*, BOOL*, DWORD*) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE UpdateUIElement(DWORD) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE EndUIElement(DWORD id) override {
        ++endCount;
        lastEndedId = id;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE EnumUIElements(IEnumTfUIElements**) override {
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetUIElement(DWORD, ITfUIElement**) override {
        return E_NOTIMPL;
    }

    int endCount = 0;
    DWORD lastEndedId = TF_INVALID_UIELEMENTID;

private:
    friend class yume::utils::ComObjectBase<FakeUiElementMgr>;
    ~FakeUiElementMgr() = default;
};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

KeyEvent key(KeyCode code, std::optional<char16_t> ch, bool ctrl = false) {
    return KeyEvent(code, ch, true, false, ctrl, false);
}

DictionaryStoragePaths makeEngineStoragePaths() {
    static int counter = 0;
    const auto dir = std::filesystem::path("build") / "selftest_engine" / std::to_string(counter++);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        throw std::runtime_error("failed to prepare selftest engine directory");
    }

    DictionaryStoragePaths paths;
    paths.userDbPath = dir / "user_db.bin";
    paths.blackDbPath = dir / "black_db.bin";
    return paths;
}

ImeEngine makeEngine() {
    return ImeEngine(makeEngineStoragePaths());
}

bool runEngineFlowTests() {
    auto engine = makeEngine();

    if (!expect(engine.getCurrentState() == ImeState::Direct, "initial state should be Direct")) {
        return false;
    }

    const auto toggle = engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    if (!expect(toggle.nextState == ImeState::Idle, "Ctrl+Space should enter Idle")) {
        return false;
    }

    const auto outK = engine.processKeyEvent(key(KeyCode::K, u'k'));
    if (!expect(outK.composition.has_value() && outK.composition->text == u"k", "k should start composition")) {
        return false;
    }

    const auto outA = engine.processKeyEvent(key(KeyCode::A, u'a'));
    if (!expect(outA.composition.has_value() && outA.composition->text == u"\u304B", "ka should become \u304b")) {
        return false;
    }

    const auto outSpace = engine.processKeyEvent(key(KeyCode::Space, std::nullopt));
    if (outSpace.nextState == ImeState::Composing) {
        if (!expect(
                outSpace.candidates.has_value() &&
                outSpace.candidates->items &&
                !outSpace.candidates->items->empty(),
                "Space should keep visible predictions when available")) {
            return false;
        }
        const KeyEvent shiftSpace(KeyCode::Space, std::nullopt, true, true, false, false);
        const auto converting = engine.processKeyEvent(shiftSpace);
        if (!expect(converting.nextState == ImeState::Converting, "Shift+Space should enter converting")) {
            return false;
        }
        if (!expect(
                converting.candidates.has_value() &&
                converting.candidates->items &&
                !converting.candidates->items->empty(),
                "conversion should expose candidates")) {
            return false;
        }
    } else {
        if (!expect(outSpace.nextState == ImeState::Converting, "Space should enter converting")) {
            return false;
        }
        if (!expect(
                outSpace.candidates.has_value() &&
                outSpace.candidates->items &&
                !outSpace.candidates->items->empty(),
                "conversion should expose candidates")) {
            return false;
        }
    }

    const auto outEnter = engine.processKeyEvent(key(KeyCode::Enter, std::nullopt));
    if (!expect(
            outEnter.nextState == ImeState::Committed && outEnter.commit.has_value(),
            "Enter in converting should commit")) {
        return false;
    }

    return true;
}

bool runNRuleTests() {
    auto engine = makeEngine();
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));

    engine.processKeyEvent(key(KeyCode::K, u'k'));
    engine.processKeyEvent(key(KeyCode::A, u'a'));
    engine.processKeyEvent(key(KeyCode::N, u'n'));
    engine.processKeyEvent(key(KeyCode::Y, u'y'));
    const auto outA = engine.processKeyEvent(key(KeyCode::A, u'a'));

    if (!expect(outA.composition.has_value() && outA.composition->text == u"\u304B\u306B\u3083", "kanya should prefer \u304b\u306b\u3083")) {
        return false;
    }

    auto apostropheEngine = makeEngine();
    apostropheEngine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    apostropheEngine.processKeyEvent(key(KeyCode::K, u'k'));
    apostropheEngine.processKeyEvent(key(KeyCode::A, u'a'));
    apostropheEngine.processKeyEvent(key(KeyCode::N, u'n'));
    apostropheEngine.processKeyEvent(key(KeyCode::Unknown, u'\''));
    const auto explicitN = apostropheEngine.processKeyEvent(key(KeyCode::A, u'a'));

    return expect(
        explicitN.composition.has_value() && explicitN.composition->text == u"\u304B\u3093\u3042",
        "kan'a should produce \u304b\u3093\u3042");
}

bool runPredictionTests() {
    auto engine = makeEngine();
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(KeyCode::K, u'k'));
    engine.processKeyEvent(key(KeyCode::Y, u'y'));
    engine.processKeyEvent(key(KeyCode::O, u'o'));

    engine.processKeyEvent(key(KeyCode::U, u'u'));
    const auto opened = engine.processKeyEvent(key(KeyCode::Down, std::nullopt));
    if (!expect(
            opened.candidates.has_value() &&
            opened.candidates->items &&
            !opened.candidates->items->empty(),
            "Down should expose prediction candidates")) {
        return false;
    }

    const auto expectedTop = opened.candidates->items->at(static_cast<size_t>(opened.candidates->selectedIndex)).text;
    const auto alt = engine.processKeyEvent(key(KeyCode::Down, std::nullopt));
    if (!expect(
            alt.candidates.has_value() &&
            alt.candidates->items &&
            !alt.candidates->items->empty(),
            "Down should keep prediction candidates visible")) {
        return false;
    }
    const auto expectedSelected = alt.candidates->items->at(static_cast<size_t>(alt.candidates->selectedIndex)).text;
    if (!expect(expectedSelected != expectedTop, "Down should move prediction selection")) {
        return false;
    }

    const auto committed = engine.processKeyEvent(key(KeyCode::Enter, std::nullopt));
    return expect(
        committed.commit.has_value() && *committed.commit == expectedSelected,
        "Enter should commit selected prediction");
}

bool runCandidateUiActionTests() {
    auto predictionEngine = makeEngine();
    predictionEngine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    predictionEngine.processKeyEvent(key(KeyCode::K, u'k'));
    predictionEngine.processKeyEvent(key(KeyCode::Y, u'y'));
    predictionEngine.processKeyEvent(key(KeyCode::O, u'o'));
    predictionEngine.processKeyEvent(key(KeyCode::U, u'u'));
    const auto opened = predictionEngine.processKeyEvent(key(KeyCode::Down, std::nullopt));
    if (!expect(
            opened.candidates.has_value() &&
            opened.candidates->items &&
            !opened.candidates->items->empty(),
            "Down should expose prediction candidates before finalize")) {
        return false;
    }
    const auto expectedFinalized = opened.candidates->items->at(
        static_cast<size_t>(opened.candidates->selectedIndex)).text;

    const auto finalized = predictionEngine.finalizeCandidateSelection();
    if (!expect(
            finalized.has_value() && finalized->commit.has_value() && *finalized->commit == expectedFinalized,
            "finalizeCandidateSelection should commit the selected prediction")) {
        return false;
    }

    auto abortEngine = makeEngine();
    abortEngine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    abortEngine.processKeyEvent(key(KeyCode::K, u'k'));
    abortEngine.processKeyEvent(key(KeyCode::Y, u'y'));
    abortEngine.processKeyEvent(key(KeyCode::O, u'o'));
    abortEngine.processKeyEvent(key(KeyCode::Down, std::nullopt));
    const auto aborted = abortEngine.abortCandidateSelection();

    return expect(
        aborted.has_value() && aborted->nextState == ImeState::Composing && aborted->shouldCloseCandidates(),
        "abortCandidateSelection should close predictions and keep composition active");
}

bool runEscapeClosesPredictionTests() {
    auto engine = makeEngine();
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(KeyCode::K, u'k'));
    engine.processKeyEvent(key(KeyCode::Y, u'y'));
    engine.processKeyEvent(key(KeyCode::O, u'o'));

    const auto opened = engine.processKeyEvent(key(KeyCode::Down, std::nullopt));
    if (!expect(
            opened.candidates.has_value() &&
            opened.candidates->items &&
            !opened.candidates->items->empty(),
            "Down should expose prediction candidates before Escape")) {
        return false;
    }

    const auto escaped = engine.processKeyEvent(key(KeyCode::Escape, std::nullopt));
    return expect(
        escaped.nextState == ImeState::Composing &&
            escaped.isConsumed &&
            escaped.shouldCloseCandidates() &&
            escaped.composition.has_value() &&
            escaped.composition->text == u"\u304D\u3087",
        "Escape should close predictions and keep composition active");
}

bool runCursorAndPunctuationTests() {
    auto engine = makeEngine();
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(KeyCode::K, u'k'));

    const auto left = engine.processKeyEvent(key(KeyCode::Left, std::nullopt));
    if (!expect(left.composition.has_value() && left.nextState == ImeState::Composing, "Left should keep composition active")) {
        return false;
    }

    auto punctuationEngine = makeEngine();
    punctuationEngine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    const auto comma = punctuationEngine.processKeyEvent(key(KeyCode::Unknown, u','));
    return expect(comma.commit.has_value() && *comma.commit == u"\u3001", "comma should commit Japanese punctuation");
}

bool runPartialConversionTests() {
    auto engine = makeEngine();
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    for (const auto event : {
             key(KeyCode::K, u'k'),
             key(KeyCode::Y, u'y'),
             key(KeyCode::O, u'o'),
             key(KeyCode::U, u'u'),
             key(KeyCode::H, u'h'),
             key(KeyCode::A, u'a'),
         }) {
        engine.processKeyEvent(event);
    }

    auto converting = engine.processKeyEvent(key(KeyCode::Space, std::nullopt));
    if (converting.nextState == ImeState::Composing) {
        converting = engine.processKeyEvent(KeyEvent(KeyCode::Space, std::nullopt, true, true, false, false));
    }
    if (!expect(converting.nextState == ImeState::Converting && converting.composition.has_value(), "Space should enter segmented conversion")) {
        return false;
    }
    if (!expect(converting.composition->segmentStart == 0 && converting.composition->segmentEnd == 2, "first segment should cover \u304d\u3087\u3046")) {
        return false;
    }

    const auto nextSegment = engine.processKeyEvent(key(KeyCode::Enter, std::nullopt));
    if (!expect(nextSegment.nextState == ImeState::Converting && nextSegment.composition.has_value(), "Enter should advance to next segment")) {
        return false;
    }
    if (!expect(nextSegment.composition->segmentStart == 2 && nextSegment.composition->segmentEnd == 3, "second segment should cover \u306f")) {
        return false;
    }

    std::optional<yume::ime::engine::CommitText> committedText;
    for (int attempt = 0; attempt < 12 && !committedText.has_value(); ++attempt) {
        const auto committed = engine.processKeyEvent(key(KeyCode::Enter, std::nullopt));
        committedText = committed.commit;
    }
    if (!committedText.has_value()) {
        const auto finalized = engine.finalizeCandidateSelection();
        if (finalized.has_value()) {
            committedText = finalized->commit;
        }
    }
    return expect(committedText.has_value() && *committedText == u"\u304D\u3087\u3046\u306F", "partial conversion should commit \u304d\u3087\u3046\u306f");
}

bool runDictionaryAwareSegmentationTests() {
    auto engine = makeEngine();
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    for (const auto event : {
             key(KeyCode::W, u'w'),
             key(KeyCode::A, u'a'),
             key(KeyCode::T, u't'),
             key(KeyCode::A, u'a'),
             key(KeyCode::S, u's'),
             key(KeyCode::H, u'h'),
             key(KeyCode::I, u'i'),
             key(KeyCode::H, u'h'),
             key(KeyCode::A, u'a'),
         }) {
        engine.processKeyEvent(event);
    }

    auto converting = engine.processKeyEvent(key(KeyCode::Space, std::nullopt));
    if (converting.nextState == ImeState::Composing) {
        converting = engine.processKeyEvent(KeyEvent(KeyCode::Space, std::nullopt, true, true, false, false));
    }
    if (!expect(converting.composition.has_value(), "watashiha should produce a composition")) {
        return false;
    }
    if (!expect(converting.composition->segmentStart == 0 && converting.composition->segmentEnd == 1, "first segment should prefer \u79c1")) {
        return false;
    }

    const auto moveRight = engine.processKeyEvent(key(KeyCode::Right, std::nullopt));
    if (!expect(moveRight.composition.has_value(), "Right should move to the next segment")) {
        return false;
    }
    if (!expect(
            moveRight.composition->segmentStart >= converting.composition->segmentEnd &&
                moveRight.composition->segmentEnd > moveRight.composition->segmentStart,
            "Right should advance to a later segment")) {
        return false;
    }

    const auto moveLeft = engine.processKeyEvent(key(KeyCode::Left, std::nullopt));
    return expect(moveLeft.composition.has_value() && moveLeft.composition->segmentStart == 0, "Left should return to the first segment");
}

bool runDictionaryLayerTests() {
    Dictionary dictionary;
    dictionary.recordCommit(u"\u306B\u307B\u3093\u3054", u"\u65E5\u672C\u8A9E");

    const auto learnedPredictions = dictionary.getPredictionsShared(u"\u306B");
    if (!expect(learnedPredictions && !learnedPredictions->empty(), "predictions should be available after learning")) {
        return false;
    }
    if (!expect(learnedPredictions->front().text == u"\u65E5\u672C\u8A9E", "user_db should promote learned prediction")) {
        return false;
    }

    dictionary.blockCandidate(u"\u304D\u3087\u3046", u"\u4ECA\u65E5");
    const auto blockedCandidates = dictionary.getCandidatesShared(u"\u304D\u3087\u3046");
    if (!expect(blockedCandidates && !blockedCandidates->empty(), "blocked candidate list should still exist")) {
        return false;
    }
    for (const auto& candidate : *blockedCandidates) {
        if (candidate.text == u"\u4ECA\u65E5") {
            return expect(false, "black_db should filter blocked candidates");
        }
    }

    dictionary.blockCandidate(u"\u308F\u305F\u3057", u"\u79C1");
    dictionary.blockCandidate(u"\u308F\u305F\u3057", u"\u6E21\u3057");
    if (!expect(!dictionary.hasExactReading(u"\u308F\u305F\u3057"), "black_db should affect exact reading detection")) {
        return false;
    }

    return true;
}

bool runPersistenceTests() {
    namespace fs = std::filesystem;
    const fs::path tempDir = fs::path("build") / "selftest_db";
    const auto userPath = tempDir / "user_db.bin";
    const auto blackPath = tempDir / "black_db.bin";
    std::error_code ec;
    fs::remove(userPath, ec);
    fs::remove(blackPath, ec);
    fs::remove_all(tempDir, ec);

    DictionaryStoragePaths paths;
    paths.userDbPath = userPath;
    paths.blackDbPath = blackPath;

    {
        Dictionary dictionary(paths);
        dictionary.recordCommit(u"\u306B\u307B\u3093\u3054", u"\u65E5\u672C\u8A9E");
        dictionary.blockCandidate(u"\u304D\u3087\u3046", u"\u4ECA\u65E5");
    }

    Dictionary reloaded(paths);
    reloaded.servicePersistence();
    const auto predictions = reloaded.getPredictionsShared(u"\u306B");
    if (!expect(predictions && !predictions->empty() && predictions->front().text == u"\u65E5\u672C\u8A9E", "user_db should persist learned entries")) {
        return false;
    }

    const auto blockedCandidates = reloaded.getCandidatesShared(u"\u304D\u3087\u3046");
    if (!expect(blockedCandidates && !blockedCandidates->empty(), "reloaded blocked candidates should exist")) {
        return false;
    }
    for (const auto& candidate : *blockedCandidates) {
        if (candidate.text == u"\u4ECA\u65E5") {
            return expect(false, "black_db should persist blocked entries");
        }
    }

    fs::remove(userPath, ec);
    fs::remove(blackPath, ec);
    fs::remove_all(tempDir, ec);
    return true;
}

bool runCorruptDbRecoveryTests() {
    namespace fs = std::filesystem;
    const fs::path tempDir = fs::path("build") / "selftest_corrupt_db";
    const auto userPath = tempDir / "user_db.bin";
    const auto blackPath = tempDir / "black_db.bin";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);
    if (!expect(!ec, "selftest should create corrupt db directory")) {
        return false;
    }

    {
        std::ofstream user(userPath, std::ios::binary | std::ios::trunc);
        if (!user.is_open()) {
            return expect(false, "selftest should open corrupt user db file");
        }
        user << "corrupt-user";
        if (!user.good()) {
            return expect(false, "selftest should write corrupt user db file");
        }
        std::ofstream black(blackPath, std::ios::binary | std::ios::trunc);
        if (!black.is_open()) {
            return expect(false, "selftest should open corrupt black db file");
        }
        black << "corrupt-black";
        if (!black.good()) {
            return expect(false, "selftest should write corrupt black db file");
        }
    }

    DictionaryStoragePaths paths;
    paths.userDbPath = userPath;
    paths.blackDbPath = blackPath;

    Dictionary recovered(paths);
    recovered.recordCommit(u"\u306B\u307B\u3093", u"\u65E5\u672C");
    if (!expect(recovered.flush(), "recovered dictionary should flush after corrupt db quarantine")) {
        fs::remove_all(tempDir, ec);
        return false;
    }

    bool foundUserBackup = false;
    bool foundBlackBackup = false;
    fs::directory_iterator it(tempDir, ec);
    if (!expect(!ec, "selftest should enumerate corrupt db directory")) {
        fs::remove_all(tempDir, ec);
        return false;
    }
    for (const auto& entry : it) {
        const auto name = entry.path().filename().string();
        if (name.find("user_db.bin.corrupt.") == 0) {
            foundUserBackup = true;
        }
        if (name.find("black_db.bin.corrupt.") == 0) {
            foundBlackBackup = true;
        }
    }

    fs::remove_all(tempDir, ec);
    return expect(foundUserBackup && foundBlackBackup, "corrupt db files should be quarantined");
}

bool runLoggerSmokeTests() {
    namespace fs = std::filesystem;
    const auto logDir = fs::path("build") / "selftest_logger";
    const auto configPath = logDir / "logging_config.json";
    const auto logPath = logDir / "logs" / "yume-ime.log";
    std::error_code ec;
    fs::remove_all(logDir, ec);
    ec.clear();
    fs::remove(logPath, ec);

    yume::utils::Logger::instance().setPathsForTesting(logPath, configPath);
    YUME_LOG_ERROR("SelfTest", "logger smoke");
    if (!expect(fs::exists(logPath, ec) && !ec, "logger should create log file")) {
        yume::utils::Logger::instance().resetForTesting();
        return false;
    }

    std::ifstream input(logPath, std::ios::binary);
    const std::string content(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    yume::utils::Logger::instance().resetForTesting();
    fs::remove_all(logDir, ec);
    return expect(content.find("logger smoke") != std::string::npos, "logger should write error message");
}

bool runIdleServiceTests() {
    auto engine = makeEngine();
    engine.processKeyEvent(key(KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(KeyCode::K, u'k'));
    engine.processKeyEvent(key(KeyCode::Y, u'y'));
    engine.processKeyEvent(key(KeyCode::O, u'o'));
    engine.processKeyEvent(key(KeyCode::U, u'u'));
    engine.serviceIdle();
    return expect(true, "serviceIdle should be callable during composition");
}

bool runTsfLifecycleTests() {
    auto& moduleState = yume::platform::tsf::ModuleState::instance();
    const auto initialObjects = moduleState.objectCount();
    auto firstSession = moduleState.tryAcquireSession();
    if (!expect(firstSession.has_value(), "first TSF session should acquire")) {
        return false;
    }
    if (!expect(moduleState.sessionCount() == 1, "first TSF session should start idle service")) {
        return false;
    }

    auto secondSession = moduleState.tryAcquireSession();
    if (!expect(secondSession.has_value(), "second TSF session should acquire")) {
        return false;
    }
    if (!expect(moduleState.sessionCount() == 2, "second TSF session should increment refcount")) {
        return false;
    }

    secondSession.reset();
    if (!expect(moduleState.sessionCount() == 1, "release should decrement TSF refcount")) {
        return false;
    }

    firstSession.reset();
    if (!expect(moduleState.sessionCount() == 0, "last TSF session should stop idle service")) {
        return false;
    }
    return expect(moduleState.objectCount() == initialObjects, "TSF lifecycle test should not leak COM objects");
}

bool runTextServiceInterfaceTests() {
    auto service = yume::utils::makeComPtr<yume::platform::tsf::TextService>();
    if (!expect(service != nullptr, "TextService should be constructible")) {
        return false;
    }

    Microsoft::WRL::ComPtr<ITfKeyEventSink> keyEventSink;
    const HRESULT keySinkHr = service->QueryInterface(
        IID_ITfKeyEventSink,
        reinterpret_cast<void**>(keyEventSink.GetAddressOf()));
    if (!expect(SUCCEEDED(keySinkHr) && keyEventSink != nullptr, "TextService should expose ITfKeyEventSink")) {
        return false;
    }

    Microsoft::WRL::ComPtr<ITfThreadMgrEventSink> threadMgrEventSink;
    const HRESULT threadMgrSinkHr = service->QueryInterface(
        IID_ITfThreadMgrEventSink,
        reinterpret_cast<void**>(threadMgrEventSink.GetAddressOf()));
    if (!expect(
            SUCCEEDED(threadMgrSinkHr) && threadMgrEventSink != nullptr,
            "TextService should expose ITfThreadMgrEventSink")) {
        return false;
    }

    Microsoft::WRL::ComPtr<ITfCompositionSink> compositionSink;
    const HRESULT compositionSinkHr = service->QueryInterface(
        IID_ITfCompositionSink,
        reinterpret_cast<void**>(compositionSink.GetAddressOf()));
    if (!expect(
            SUCCEEDED(compositionSinkHr) && compositionSink != nullptr,
            "TextService should expose ITfCompositionSink")) {
        return false;
    }

    Microsoft::WRL::ComPtr<ITfUIElementSink> uiElementSink;
    const HRESULT uiElementSinkHr = service->QueryInterface(
        IID_ITfUIElementSink,
        reinterpret_cast<void**>(uiElementSink.GetAddressOf()));
    if (!expect(
            SUCCEEDED(uiElementSinkHr) && uiElementSink != nullptr,
            "TextService should expose ITfUIElementSink")) {
        return false;
    }
    return true;
}

bool runTextServiceRegistrationTests() {
    auto source = yume::utils::makeComPtr<FakeSource>();
    if (!expect(source != nullptr, "registration test should create fake ITfSource")) {
        return false;
    }

    tsf::TextServiceRegistrationTestAccessor::ContextSinkRegistration movedSink;
    {
        tsf::TextServiceRegistrationTestAccessor::ContextSinkRegistration registration;
        registration.source = source;
        registration.cookie = 42;

        movedSink = std::move(registration);
        if (!expect(registration.source.Get() == nullptr, "moved-from context sink should release source")) {
            return false;
        }
        if (!expect(registration.cookie == TF_INVALID_COOKIE, "moved-from context sink should clear cookie")) {
            return false;
        }
    }

    if (!expect(source->unadviseCount == 0, "moved-to context sink should own the unadvise call")) {
        return false;
    }
    movedSink.reset();
    if (!expect(source->unadviseCount == 1 && source->lastCookie == 42, "context sink reset should unadvise exactly once")) {
        return false;
    }

    auto manager = yume::utils::makeComPtr<FakeUiElementMgr>();
    if (!expect(manager != nullptr, "registration test should create fake ITfUIElementMgr")) {
        return false;
    }

    tsf::TextServiceRegistrationTestAccessor::UiElementRegistration movedUi;
    {
        tsf::TextServiceRegistrationTestAccessor::UiElementRegistration registration;
        registration.manager = manager;
        registration.id = 7;

        movedUi = std::move(registration);
        if (!expect(registration.manager.Get() == nullptr, "moved-from ui registration should release manager")) {
            return false;
        }
        if (!expect(!registration.isActive(), "moved-from ui registration should become inactive")) {
            return false;
        }
    }

    if (!expect(manager->endCount == 0, "moved-to ui registration should own the end call")) {
        return false;
    }
    movedUi.reset();
    return expect(
        manager->endCount == 1 && manager->lastEndedId == 7,
        "ui registration reset should end the ui element exactly once");
}

} // namespace

int main() {
    if (!runEngineFlowTests()) {
        return EXIT_FAILURE;
    }
    if (!runNRuleTests()) {
        return EXIT_FAILURE;
    }
    if (!runPredictionTests()) {
        return EXIT_FAILURE;
    }
    if (!runCandidateUiActionTests()) {
        return EXIT_FAILURE;
    }
    if (!runEscapeClosesPredictionTests()) {
        return EXIT_FAILURE;
    }
    if (!runCursorAndPunctuationTests()) {
        return EXIT_FAILURE;
    }
    if (!runPartialConversionTests()) {
        return EXIT_FAILURE;
    }
    if (!runDictionaryAwareSegmentationTests()) {
        return EXIT_FAILURE;
    }
    if (!runDictionaryLayerTests()) {
        return EXIT_FAILURE;
    }
    if (!runPersistenceTests()) {
        return EXIT_FAILURE;
    }
    if (!runCorruptDbRecoveryTests()) {
        return EXIT_FAILURE;
    }
    if (!runLoggerSmokeTests()) {
        return EXIT_FAILURE;
    }
    if (!runIdleServiceTests()) {
        return EXIT_FAILURE;
    }
    if (!runTsfLifecycleTests()) {
        return EXIT_FAILURE;
    }
    if (!runTextServiceInterfaceTests()) {
        return EXIT_FAILURE;
    }
    if (!runTextServiceRegistrationTests()) {
        return EXIT_FAILURE;
    }

    std::cout << "ImeCoreSelfTests passed\n";
    return EXIT_SUCCESS;
}
