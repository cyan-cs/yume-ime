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



#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <vector>

#include "ime/dictionary/black_db.hpp"
#include "ime/dictionary/dictionary.hpp"
#include "ime/dictionary/user_db.hpp"
#include "ime/engine/ime_engine.hpp"
#include "ime/input/key_event.hpp"
#include "ime/state/ime_states.hpp"
#include "utils/app_paths.hpp"

using namespace yume::ime;

namespace {

input::KeyEvent key(input::KeyCode code, std::optional<char16_t> ch = std::nullopt, bool ctrl = false) {
    return input::KeyEvent(code, ch, true, false, ctrl, false);
}

std::filesystem::path makeDictionaryTempDir(const char* name) {
    const auto dir = std::filesystem::path("build") / "gtest_db" / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        throw std::runtime_error("failed to prepare dictionary temp directory");
    }
    return dir;
}

void writeBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::error_code ec;
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            throw std::runtime_error("failed to prepare parent directory for test bytes");
        }
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open test byte file");
    }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output.good()) {
        throw std::runtime_error("failed to write test byte file");
    }
}

std::vector<std::filesystem::path> findCorruptBackups(const std::filesystem::path& dir, const char* stem) {
    std::vector<std::filesystem::path> found;
    if (!std::filesystem::exists(dir)) {
        return found;
    }

    const std::string prefix = std::string(stem) + ".corrupt.";
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.rfind(prefix, 0) == 0) {
            found.push_back(entry.path());
        }
    }
    return found;
}

dictionary::DictionaryStoragePaths makeEngineStoragePaths() {
    static int counter = 0;
    const auto dir = std::filesystem::path("build") / "gtest_engine" / std::to_string(counter++);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        throw std::runtime_error("failed to prepare engine temp directory");
    }

    dictionary::DictionaryStoragePaths paths;
    paths.userDbPath = dir / "user_db.bin";
    paths.blackDbPath = dir / "black_db.bin";
    return paths;
}

engine::ImeEngine makeEngine() {
    return engine::ImeEngine(makeEngineStoragePaths());
}

}

TEST(ImeEngineTest, InitialStateIsDirect) {
    auto engine = makeEngine();
    EXPECT_EQ(engine.getCurrentState(), state::ImeState::Direct);
}

TEST(ImeEngineTest, CtrlSpaceTogglesIdleMode) {
    auto engine = makeEngine();
    auto output = engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    EXPECT_TRUE(output.isConsumed);
    EXPECT_EQ(output.nextState, state::ImeState::Idle);
    EXPECT_EQ(engine.getCurrentState(), state::ImeState::Idle);
}

TEST(ImeEngineTest, ToggleImeKeySwitchesBetweenDirectAndIdle) {
    auto engine = makeEngine();

    auto idle = engine.processKeyEvent(key(input::KeyCode::ToggleIme));
    EXPECT_TRUE(idle.isConsumed);
    EXPECT_EQ(idle.nextState, state::ImeState::Idle);

    auto direct = engine.processKeyEvent(key(input::KeyCode::ToggleIme));
    EXPECT_TRUE(direct.isConsumed);
    EXPECT_EQ(direct.nextState, state::ImeState::Direct);
}

TEST(ImeEngineTest, ModeKeysSwitchBetweenLatinAndHiragana) {
    auto engine = makeEngine();

    auto hiragana = engine.processKeyEvent(key(input::KeyCode::ModeHiragana));
    EXPECT_TRUE(hiragana.isConsumed);
    EXPECT_EQ(hiragana.nextState, state::ImeState::Idle);

    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    auto latin = engine.processKeyEvent(key(input::KeyCode::ModeLatin));
    EXPECT_TRUE(latin.isConsumed);
    EXPECT_EQ(latin.nextState, state::ImeState::Direct);
    EXPECT_FALSE(latin.composition.has_value());
}

TEST(ImeEngineTest, KaBecomesHiraganaComposition) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    ASSERT_TRUE(outA.composition.has_value());
    EXPECT_EQ(outA.composition->text, u"\u304B");
    EXPECT_EQ(outA.composition->cursorPosition, 1u);
    EXPECT_EQ(outA.nextState, state::ImeState::Composing);
    ASSERT_TRUE(outA.candidates.has_value());
    ASSERT_TRUE(outA.candidates->items);
    ASSERT_FALSE(outA.candidates->items->empty());
}

TEST(ImeEngineTest, FullWidthAsciiUsesSameCompositionPathAsHalfWidthAscii) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'\uFF4B'));
    auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'\uFF41'));

    ASSERT_TRUE(outA.composition.has_value());
    EXPECT_EQ(outA.composition->text, u"\u304B");
    ASSERT_TRUE(outA.candidates.has_value());
    ASSERT_TRUE(outA.candidates->items);
    ASSERT_FALSE(outA.candidates->items->empty());
}

TEST(ImeEngineTest, ShiftedAsciiCommitsUppercaseDirectly) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    const auto outK = engine.processKeyEvent(
        input::KeyEvent(input::KeyCode::K, u'K', true, true, false, false));
    ASSERT_TRUE(outK.commit.has_value());
    EXPECT_EQ(*outK.commit, u"K");
    EXPECT_EQ(outK.nextState, state::ImeState::Committed);
    EXPECT_FALSE(outK.composition.has_value());
}

TEST(ImeEngineTest, ShiftedAsciiCommitsAfterExistingComposition) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));

    const auto outA = engine.processKeyEvent(
        input::KeyEvent(input::KeyCode::A, u'A', true, true, false, false));
    ASSERT_TRUE(outA.commit.has_value());
    EXPECT_EQ(*outA.commit, u"kA");
    EXPECT_EQ(outA.nextState, state::ImeState::Committed);
}

TEST(ImeEngineTest, EnterCommitsPendingRomaji) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));

    auto output = engine.processKeyEvent(key(input::KeyCode::Enter));

    ASSERT_TRUE(output.commit.has_value());
    EXPECT_EQ(*output.commit, u"k");
    EXPECT_EQ(output.nextState, state::ImeState::Committed);
}

TEST(ImeEngineTest, LeftAndRightMoveCompositionCursor) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));

    auto left = engine.processKeyEvent(key(input::KeyCode::Left));
    ASSERT_TRUE(left.composition.has_value());
    EXPECT_EQ(left.composition->cursorPosition, 0u);

    auto right = engine.processKeyEvent(key(input::KeyCode::Right));
    ASSERT_TRUE(right.composition.has_value());
    EXPECT_EQ(right.composition->cursorPosition, 1u);
}

TEST(ImeEngineTest, DeleteRemovesCharacterAtCursor) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Left));

    auto output = engine.processKeyEvent(key(input::KeyCode::Delete));

    EXPECT_EQ(output.nextState, state::ImeState::Idle);
    EXPECT_FALSE(output.composition.has_value());
}

TEST(ImeEngineTest, PunctuationCommitsJapaneseMarksInHiraganaMode) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    auto comma = engine.processKeyEvent(key(input::KeyCode::Unknown, u','));
    ASSERT_TRUE(comma.commit.has_value());
    EXPECT_EQ(*comma.commit, u"\u3001");
    EXPECT_EQ(comma.nextState, state::ImeState::Committed);
}

TEST(ImeEngineTest, HyphenCommitsLongVowelMarkInHiraganaMode) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    auto hyphen = engine.processKeyEvent(key(input::KeyCode::Unknown, u'-'));

    ASSERT_TRUE(hyphen.commit.has_value());
    EXPECT_EQ(*hyphen.commit, u"\u30FC");
    EXPECT_EQ(hyphen.nextState, state::ImeState::Committed);
}

TEST(ImeEngineTest, HyphenExtendsCompositionWithoutFixingPrediction) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    const auto visible = engine.processKeyEvent(key(input::KeyCode::U, u'u'));

    ASSERT_TRUE(visible.candidates.has_value());
    ASSERT_TRUE(visible.candidates->items);
    ASSERT_FALSE(visible.candidates->items->empty());

    const auto hyphen = engine.processKeyEvent(key(input::KeyCode::Unknown, u'-'));

    EXPECT_EQ(hyphen.nextState, state::ImeState::Composing);
    EXPECT_FALSE(hyphen.commit.has_value());
    ASSERT_TRUE(hyphen.composition.has_value());
    EXPECT_EQ(hyphen.composition->text, u"\u304D\u3087\u3046\u30FC");
    ASSERT_TRUE(hyphen.candidates.has_value());
}

TEST(ImeEngineTest, UnsupportedSymbolCommitsDirectlyWithoutStartingComposition) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    auto bracket = engine.processKeyEvent(key(input::KeyCode::Unknown, u'['));

    ASSERT_TRUE(bracket.commit.has_value());
    EXPECT_EQ(*bracket.commit, u"[");
    EXPECT_EQ(bracket.nextState, state::ImeState::Committed);
    EXPECT_FALSE(bracket.composition.has_value());
}

TEST(ImeEngineTest, BracesCommitDirectlyWithoutStartingComposition) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    auto leftBrace = engine.processKeyEvent(key(input::KeyCode::Unknown, u'{'));

    ASSERT_TRUE(leftBrace.commit.has_value());
    EXPECT_EQ(*leftBrace.commit, u"{");
    EXPECT_EQ(leftBrace.nextState, state::ImeState::Committed);
    EXPECT_FALSE(leftBrace.composition.has_value());
}

TEST(ImeEngineTest, FullWidthSymbolCommitsDirectlyWithoutStartingComposition) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    auto bracket = engine.processKeyEvent(key(input::KeyCode::Unknown, u'\uFF3B'));

    ASSERT_TRUE(bracket.commit.has_value());
    EXPECT_EQ(*bracket.commit, u"[");
    EXPECT_EQ(bracket.nextState, state::ImeState::Committed);
    EXPECT_FALSE(bracket.composition.has_value());
}

TEST(ImeEngineTest, YoonRulesRemainSupported) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    ASSERT_TRUE(outA.composition.has_value());
    EXPECT_EQ(outA.composition->text, u"\u304D\u3083");
}

TEST(ImeEngineTest, SiAliasProducesShi) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::S, u's'));
    auto outI = engine.processKeyEvent(key(input::KeyCode::I, u'i'));

    ASSERT_TRUE(outI.composition.has_value());
    EXPECT_EQ(outI.composition->text, u"\u3057");
}

TEST(ImeEngineTest, TiAliasProducesChi) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::T, u't'));
    auto outI = engine.processKeyEvent(key(input::KeyCode::I, u'i'));

    ASSERT_TRUE(outI.composition.has_value());
    EXPECT_EQ(outI.composition->text, u"\u3061");
}

TEST(ImeEngineTest, SyuAliasProducesShu) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::S, u's'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    auto outU = engine.processKeyEvent(key(input::KeyCode::U, u'u'));

    ASSERT_TRUE(outU.composition.has_value());
    EXPECT_EQ(outU.composition->text, u"\u3057\u3085");
}

TEST(ImeEngineTest, NApostropheAProducesNThenA) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::N, u'n'));
    engine.processKeyEvent(key(input::KeyCode::Unknown, u'\''));
    auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    ASSERT_TRUE(outA.composition.has_value());
    EXPECT_EQ(outA.composition->text, u"\u3093\u3042");
}

TEST(ImeEngineTest, JyaAliasProducesJa) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::J, u'j'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    ASSERT_TRUE(outA.composition.has_value());
    EXPECT_EQ(outA.composition->text, u"\u3058\u3083");
}

TEST(ImeEngineTest, SheProducesSheKana) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::S, u's'));
    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    auto outE = engine.processKeyEvent(key(input::KeyCode::E, u'e'));

    ASSERT_TRUE(outE.composition.has_value());
    EXPECT_EQ(outE.composition->text, u"\u3057\u3047");
}

TEST(ImeEngineTest, CheProducesCheKana) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::C, u'c'));
    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    auto outE = engine.processKeyEvent(key(input::KeyCode::E, u'e'));

    ASSERT_TRUE(outE.composition.has_value());
    EXPECT_EQ(outE.composition->text, u"\u3061\u3047");
}

TEST(ImeEngineTest, TsaProducesTsaKana) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::T, u't'));
    engine.processKeyEvent(key(input::KeyCode::S, u's'));
    auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    ASSERT_TRUE(outA.composition.has_value());
    EXPECT_EQ(outA.composition->text, u"\u3064\u3041");
}

TEST(ImeEngineTest, QwaProducesKwaKana) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::Q, u'q'));
    engine.processKeyEvent(key(input::KeyCode::W, u'w'));
    auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    ASSERT_TRUE(outA.composition.has_value());
    EXPECT_EQ(outA.composition->text, u"\u304f\u3041");
}

TEST(ImeEngineTest, WiProducesWiKana) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::W, u'w'));
    auto outI = engine.processKeyEvent(key(input::KeyCode::I, u'i'));

    ASSERT_TRUE(outI.composition.has_value());
    EXPECT_EQ(outI.composition->text, u"\u3046\u3043");
}

TEST(ImeEngineTest, XwaProducesSmallWa) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::X, u'x'));
    engine.processKeyEvent(key(input::KeyCode::W, u'w'));
    auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    ASSERT_TRUE(outA.composition.has_value());
    EXPECT_EQ(outA.composition->text, u"\u308e");
}

TEST(ImeEngineTest, SyeProducesSheKana) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::S, u's'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    auto outE = engine.processKeyEvent(key(input::KeyCode::E, u'e'));

    ASSERT_TRUE(outE.composition.has_value());
    EXPECT_EQ(outE.composition->text, u"\u3057\u3047");
}

TEST(ImeEngineTest, ThiProducesThiKana) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::T, u't'));
    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    auto outI = engine.processKeyEvent(key(input::KeyCode::I, u'i'));

    ASSERT_TRUE(outI.composition.has_value());
    EXPECT_EQ(outI.composition->text, u"\u3066\u3043");
}

TEST(ImeEngineTest, LoneNRemainsPendingUntilDisambiguated) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    auto pending = engine.processKeyEvent(key(input::KeyCode::N, u'n'));
    ASSERT_TRUE(pending.composition.has_value());
    EXPECT_EQ(pending.composition->text, u"n");

    auto committed = engine.processKeyEvent(key(input::KeyCode::N, u'n'));
    ASSERT_TRUE(committed.composition.has_value());
    EXPECT_EQ(committed.composition->text, u"\u3093");
}

TEST(ImeEngineTest, NCommitsWhenFollowedByDifferentConsonant) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::N, u'n'));

    auto output = engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    ASSERT_TRUE(output.composition.has_value());
    EXPECT_EQ(output.composition->text, u"\u3093k");
}

TEST(ImeEngineTest, BackspaceDoesNotLeaveDanglingConsonantAfterCompleteKana) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::B, u'b'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    auto output = engine.processKeyEvent(key(input::KeyCode::Backspace));
    EXPECT_EQ(output.nextState, state::ImeState::Idle);
    EXPECT_FALSE(output.composition.has_value());
}

TEST(ImeEngineTest, BackspaceRemovesCompleteTrailingTyaUnitWithoutLeavingTy) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::B, u'b'));
    engine.processKeyEvent(key(input::KeyCode::T, u't'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    const auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    ASSERT_TRUE(outA.composition.has_value());
    EXPECT_EQ(outA.composition->text, u"b\u3061\u3083");

    const auto output = engine.processKeyEvent(key(input::KeyCode::Backspace));

    EXPECT_EQ(output.nextState, state::ImeState::Composing);
    ASSERT_TRUE(output.composition.has_value());
    EXPECT_EQ(output.composition->text, u"b");
}

TEST(ImeEngineTest, PredictionsOpenWhileComposing) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    auto outO = engine.processKeyEvent(key(input::KeyCode::O, u'o'));

    ASSERT_TRUE(outO.candidates.has_value());
    ASSERT_TRUE(outO.candidates->items);
    ASSERT_FALSE(outO.candidates->items->empty());
    EXPECT_NE(outO.candidates->items->front().text, u"\u304D\u3087");
}

TEST(ImeEngineTest, ShortKanaPredictionsUseCompactCandidateLimit) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::N, u'n'));
    const auto outI = engine.processKeyEvent(key(input::KeyCode::I, u'i'));

    ASSERT_TRUE(outI.candidates.has_value());
    ASSERT_TRUE(outI.candidates->items);
    EXPECT_TRUE(outI.candidates->items->size() <= 4u);
}

TEST(ImeEngineTest, DigitsOpenPredictionsWhileComposing) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    auto out1 = engine.processKeyEvent(key(input::KeyCode::Num1, u'1'));

    EXPECT_EQ(out1.nextState, state::ImeState::Composing);
    ASSERT_TRUE(out1.composition.has_value());
    EXPECT_EQ(out1.composition->text, u"1");
    ASSERT_TRUE(out1.candidates.has_value());
    ASSERT_TRUE(out1.candidates->items);
    ASSERT_FALSE(out1.candidates->items->empty());
    EXPECT_EQ(out1.candidates->items->front().text, u"1");
}

TEST(ImeEngineTest, DigitsInsertWhilePredictionWindowIsVisible) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::Num1, u'1'));
    auto out2 = engine.processKeyEvent(key(input::KeyCode::Num2, u'2'));

    EXPECT_EQ(out2.nextState, state::ImeState::Composing);
    ASSERT_TRUE(out2.composition.has_value());
    EXPECT_EQ(out2.composition->text, u"12");
    ASSERT_TRUE(out2.candidates.has_value());
    ASSERT_TRUE(out2.candidates->items);
    ASSERT_FALSE(out2.candidates->items->empty());
    EXPECT_EQ(out2.candidates->items->front().text, u"12");
}

TEST(ImeEngineTest, MultiDigitCompositionAddsNumericFormattingCandidates) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::Num1, u'1'));
    engine.processKeyEvent(key(input::KeyCode::Num2, u'2'));
    engine.processKeyEvent(key(input::KeyCode::Num3, u'3'));
    const auto out4 = engine.processKeyEvent(key(input::KeyCode::Num4, u'4'));

    ASSERT_TRUE(out4.candidates.has_value());
    ASSERT_TRUE(out4.candidates->items);
    ASSERT_FALSE(out4.candidates->items->empty());

    const auto& items = *out4.candidates->items;
    EXPECT_NE(
        std::find_if(items.begin(), items.end(), [](const auto& candidate) {
            return candidate.text == u"\uFF11\uFF12\uFF13\uFF14";
        }),
        items.end());
    EXPECT_NE(
        std::find_if(items.begin(), items.end(), [](const auto& candidate) {
            return candidate.text == u"1,234";
        }),
        items.end());
}

TEST(ImeEngineTest, NumericPredictionsKeepExpandedCandidateLimit) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::Num1, u'1'));
    engine.processKeyEvent(key(input::KeyCode::Num2, u'2'));
    engine.processKeyEvent(key(input::KeyCode::Num3, u'3'));
    const auto out4 = engine.processKeyEvent(key(input::KeyCode::Num4, u'4'));

    ASSERT_TRUE(out4.candidates.has_value());
    ASSERT_TRUE(out4.candidates->items);
    EXPECT_TRUE(out4.candidates->items->size() >= 5u);
}

TEST(ImeEngineTest, ParticleContextMovesFallbackPredictionsBehindLexicalCandidates) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    engine.processKeyEvent(shiftSpace);
    const auto committedToday = engine.processKeyEvent(key(input::KeyCode::Num1));
    ASSERT_TRUE(committedToday.commit.has_value());
    EXPECT_EQ(*committedToday.commit, u"\u4ECA\u65E5");

    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    const auto committedTopic = engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    ASSERT_TRUE(committedTopic.composition.has_value());
    const auto committedParticle = engine.processKeyEvent(key(input::KeyCode::Enter));
    ASSERT_TRUE(committedParticle.commit.has_value());
    EXPECT_EQ(*committedParticle.commit, u"\u306F");

    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    const auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    ASSERT_TRUE(outA.candidates.has_value());
    ASSERT_TRUE(outA.candidates->items);
    ASSERT_FALSE(outA.candidates->items->empty());
    EXPECT_NE(outA.candidates->items->front().text, u"\u304B");
    EXPECT_NE(outA.candidates->items->front().text, u"ka");
}

TEST(ImeEngineTest, FullWidthDigitsNormalizeToHalfWidthWhileComposing) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    auto out1 = engine.processKeyEvent(key(input::KeyCode::Num1, u'\uFF11'));

    ASSERT_TRUE(out1.composition.has_value());
    EXPECT_EQ(out1.composition->text, u"1");
    ASSERT_TRUE(out1.candidates.has_value());
    ASSERT_TRUE(out1.candidates->items);
    ASSERT_FALSE(out1.candidates->items->empty());
    EXPECT_EQ(out1.candidates->items->front().text, u"1");
}

TEST(ImeEngineTest, InvalidRomajiKeepsInputOrder) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    const std::u16string sequence = u"qwerty";
    engine::EngineOutput output;
    for (char16_t ch : sequence) {
        const auto keyCode = static_cast<input::KeyCode>(
            static_cast<uint32_t>(input::KeyCode::A) + static_cast<uint32_t>(ch - u'a'));
        output = engine.processKeyEvent(key(keyCode, ch));
    }

    ASSERT_TRUE(output.composition.has_value());
    EXPECT_EQ(output.composition->text, u"qw\u3048rty");
}

TEST(ImeEngineTest, MixedValidAndInvalidRomajiKeepsInputOrder) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    const std::u16string sequence = u"qwertyuiop";
    engine::EngineOutput output;
    for (char16_t ch : sequence) {
        const auto keyCode = static_cast<input::KeyCode>(
            static_cast<uint32_t>(input::KeyCode::A) + static_cast<uint32_t>(ch - u'a'));
        output = engine.processKeyEvent(key(keyCode, ch));
    }

    ASSERT_TRUE(output.composition.has_value());
    EXPECT_EQ(output.composition->text, u"qw\u3048r\u3061\u3085\u3044\u304Ap");
}

TEST(ImeEngineTest, EnterCommitsRawReadingWhileComposingByDefault) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    const auto visible = engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    ASSERT_TRUE(visible.candidates.has_value());
    ASSERT_TRUE(visible.candidates->items);
    ASSERT_FALSE(visible.candidates->items->empty());

    auto output = engine.processKeyEvent(key(input::KeyCode::Enter));

    ASSERT_TRUE(output.commit.has_value());
    EXPECT_EQ(*output.commit, u"\u304D\u3087\u3046");
    EXPECT_EQ(output.nextState, state::ImeState::Committed);
}

TEST(ImeEngineTest, EnterCommitsRawReadingWhileComposing) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    const auto visible = engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    ASSERT_TRUE(visible.candidates.has_value());
    ASSERT_TRUE(visible.candidates->items);
    ASSERT_FALSE(visible.candidates->items->empty());
    auto output = engine.processKeyEvent(key(input::KeyCode::Enter));

    ASSERT_TRUE(output.commit.has_value());
    EXPECT_EQ(*output.commit, u"\u304D\u3087\u3046");
    EXPECT_EQ(output.nextState, state::ImeState::Committed);
}

TEST(ImeEngineTest, DownThenEnterCommitsAlternatePredictionWhileComposing) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    const auto visible = engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    ASSERT_TRUE(visible.candidates.has_value());
    ASSERT_TRUE(visible.candidates->items);
    ASSERT_TRUE(visible.candidates->items->size() >= 2u);
    const auto expected = visible.candidates->items->at(1).text;
    engine.processKeyEvent(key(input::KeyCode::Down));

    auto output = engine.processKeyEvent(key(input::KeyCode::Enter));

    ASSERT_TRUE(output.commit.has_value());
    EXPECT_EQ(*output.commit, expected);
}

TEST(ImeEngineTest, SecondSpaceThenEnterCommitsAlternatePredictionWhileComposing) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    const auto visible = engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    ASSERT_TRUE(visible.candidates.has_value());
    ASSERT_TRUE(visible.candidates->items);
    ASSERT_TRUE(visible.candidates->items->size() >= 2u);
    const auto expected = visible.candidates->items->at(1).text;
    engine.processKeyEvent(key(input::KeyCode::Space));
    engine.processKeyEvent(key(input::KeyCode::Space));

    auto output = engine.processKeyEvent(key(input::KeyCode::Enter));

    ASSERT_TRUE(output.commit.has_value());
    EXPECT_EQ(*output.commit, expected);
}

TEST(ImeEngineTest, TabMovesToNextPredictionCandidateWhenVisible) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));

    auto output = engine.processKeyEvent(key(input::KeyCode::Tab));

    ASSERT_TRUE(output.candidates.has_value());
    EXPECT_EQ(output.candidates->selectedIndex, 1);
}

TEST(ImeEngineTest, SpaceMovesToNextPredictionCandidateWhileComposing) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));

    const auto output = engine.processKeyEvent(key(input::KeyCode::Space));

    EXPECT_EQ(output.nextState, state::ImeState::Composing);
    EXPECT_TRUE(output.isConsumed);
    ASSERT_TRUE(output.candidates.has_value());
    EXPECT_EQ(output.candidates->selectedIndex, 0);
}

TEST(ImeEngineTest, SecondSpaceMovesToNextPredictionCandidateWhileComposing) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    engine.processKeyEvent(key(input::KeyCode::Space));

    const auto output = engine.processKeyEvent(key(input::KeyCode::Space));

    EXPECT_EQ(output.nextState, state::ImeState::Composing);
    EXPECT_TRUE(output.isConsumed);
    ASSERT_TRUE(output.candidates.has_value());
    EXPECT_EQ(output.candidates->selectedIndex, 1);
}

TEST(ImeEngineTest, SpaceDoesNotStartConversionWhenPredictionsAreUnavailable) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    for (char16_t ch : std::u16string{u"wagahaihanezumi"}) {
        engine.processKeyEvent(key(input::KeyCode::Unknown, ch));
    }

    const auto output = engine.processKeyEvent(key(input::KeyCode::Space));

    EXPECT_EQ(output.nextState, state::ImeState::Composing);
    EXPECT_TRUE(output.isConsumed);
    ASSERT_TRUE(output.composition.has_value());
    EXPECT_EQ(output.composition->text, u"\u308F\u304C\u306F\u3044\u306F\u306D\u305A\u307F");
    EXPECT_FALSE(output.commit.has_value());
}

TEST(ImeEngineTest, ShiftSpaceStartsConversionWhenPredictionsAreUnavailable) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));

    for (char16_t ch : std::u16string{u"wagahaihanezumi"}) {
        engine.processKeyEvent(key(input::KeyCode::Unknown, ch));
    }

    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    const auto output = engine.processKeyEvent(shiftSpace);

    EXPECT_EQ(output.nextState, state::ImeState::Converting);
    EXPECT_TRUE(output.isConsumed);
    ASSERT_TRUE(output.composition.has_value());
    ASSERT_TRUE(output.candidates.has_value());
}

TEST(ImeEngineTest, ExplicitNRulesRemainSupported) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::N, u'n'));
    engine.processKeyEvent(key(input::KeyCode::Unknown, u'\''));
    auto outA = engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    ASSERT_TRUE(outA.composition.has_value());
    EXPECT_EQ(outA.composition->text, u"\u304B\u3093\u3042");
}

TEST(ImeEngineTest, UnsupportedSymbolCommitsCompositionSafely) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    const auto visible = engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    ASSERT_TRUE(visible.candidates.has_value());
    ASSERT_TRUE(visible.candidates->items);
    ASSERT_FALSE(visible.candidates->items->empty());

    const auto output = engine.processKeyEvent(key(input::KeyCode::Unknown, u'['));

    ASSERT_TRUE(output.commit.has_value());
    EXPECT_EQ(*output.commit, visible.candidates->items->front().text + u"[");
    EXPECT_EQ(output.nextState, state::ImeState::Committed);
}

TEST(ImeEngineTest, PartialConversionAdvancesAcrossSegments) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    auto converting = engine.processKeyEvent(shiftSpace);
    ASSERT_TRUE(converting.composition.has_value());
    EXPECT_EQ(converting.composition->segmentStart, 0u);
    EXPECT_EQ(converting.composition->segmentEnd, 2u);

    auto nextSegment = engine.processKeyEvent(key(input::KeyCode::Enter));
    EXPECT_EQ(nextSegment.nextState, state::ImeState::Converting);
    ASSERT_TRUE(nextSegment.composition.has_value());
    EXPECT_EQ(nextSegment.composition->segmentStart, 2u);
    EXPECT_EQ(nextSegment.composition->segmentEnd, 3u);

    auto committed = engine.processKeyEvent(key(input::KeyCode::Enter));
    ASSERT_TRUE(committed.commit.has_value());
    EXPECT_EQ(*committed.commit, u"\u4ECA\u65E5\u306F");
    EXPECT_EQ(committed.nextState, state::ImeState::Committed);
}

TEST(ImeEngineTest, DictionaryAwareSegmentationPrefersKnownWordBeforeParticle) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::W, u'w'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::T, u't'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::S, u's'));
    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    engine.processKeyEvent(key(input::KeyCode::I, u'i'));
    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));

    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    auto converting = engine.processKeyEvent(shiftSpace);
    ASSERT_TRUE(converting.composition.has_value());
    EXPECT_EQ(converting.composition->segmentStart, 0u);
    EXPECT_EQ(converting.composition->segmentEnd, 1u);

    auto right = engine.processKeyEvent(key(input::KeyCode::Right));
    ASSERT_TRUE(right.composition.has_value());
    EXPECT_EQ(right.composition->segmentStart, 1u);
    EXPECT_EQ(right.composition->segmentEnd, 2u);

    auto left = engine.processKeyEvent(key(input::KeyCode::Left));
    ASSERT_TRUE(left.composition.has_value());
    EXPECT_EQ(left.composition->segmentStart, 0u);
    EXPECT_EQ(left.composition->segmentEnd, 1u);
}

TEST(ImeEngineTest, DictionaryAwareSegmentationPrefersVerbStemBeforeEnding) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::I, u'i'));
    engine.processKeyEvent(key(input::KeyCode::M, u'm'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::S, u's'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));

    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    auto converting = engine.processKeyEvent(shiftSpace);
    ASSERT_TRUE(converting.composition.has_value());
    EXPECT_EQ(converting.composition->segmentStart, 0u);
    EXPECT_EQ(converting.composition->segmentEnd, 2u);

    auto nextSegment = engine.processKeyEvent(key(input::KeyCode::Enter));
    ASSERT_TRUE(nextSegment.composition.has_value());
    EXPECT_EQ(nextSegment.composition->segmentStart, 2u);
    EXPECT_EQ(nextSegment.composition->segmentEnd, 4u);
}

TEST(ImeEngineTest, DictionaryAwareSegmentationPrefersNounBeforeCopula) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    for (char16_t ch : std::u16string{u"nihongodesu"}) {
        engine.processKeyEvent(key(input::KeyCode::Unknown, ch));
    }

    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    auto converting = engine.processKeyEvent(shiftSpace);
    ASSERT_TRUE(converting.composition.has_value());
    EXPECT_EQ(converting.composition->segmentStart, 0u);
    EXPECT_EQ(converting.composition->segmentEnd, 4u);

    auto nextSegment = engine.processKeyEvent(key(input::KeyCode::Enter));
    ASSERT_TRUE(nextSegment.composition.has_value());
    EXPECT_EQ(nextSegment.composition->segmentStart, 4u);
    EXPECT_EQ(nextSegment.composition->segmentEnd, 6u);
}

TEST(ImeEngineTest, ChangingPreviousSegmentInvalidatesFollowingSegmentCandidates) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    for (char16_t ch : std::u16string{u"nihongo"}) {
        engine.processKeyEvent(key(input::KeyCode::Unknown, ch));
    }

    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    auto converting = engine.processKeyEvent(shiftSpace);
    ASSERT_TRUE(converting.composition.has_value());

    auto secondSegment = engine.processKeyEvent(key(input::KeyCode::Enter));
    ASSERT_TRUE(secondSegment.composition.has_value());
    ASSERT_TRUE(secondSegment.candidates.has_value());
    ASSERT_TRUE(secondSegment.candidates->items);
    ASSERT_FALSE(secondSegment.candidates->items->empty());
    EXPECT_EQ(secondSegment.candidates->items->front().text, u"\u8A9E");

    auto backToFirst = engine.processKeyEvent(key(input::KeyCode::Left));
    ASSERT_TRUE(backToFirst.candidates.has_value());
    ASSERT_TRUE(backToFirst.candidates->items);
    ASSERT_TRUE(backToFirst.candidates->items->size() >= 2u);

    auto alternateFirst = engine.processKeyEvent(key(input::KeyCode::Down));
    ASSERT_TRUE(alternateFirst.candidates.has_value());
    ASSERT_TRUE(alternateFirst.candidates->items);
    EXPECT_EQ(alternateFirst.candidates->items->at(static_cast<size_t>(alternateFirst.candidates->selectedIndex)).text, u"\u4E8C\u672C");

    auto refreshedSecond = engine.processKeyEvent(key(input::KeyCode::Right));
    ASSERT_TRUE(refreshedSecond.composition.has_value());
    ASSERT_TRUE(refreshedSecond.candidates.has_value());
    ASSERT_TRUE(refreshedSecond.candidates->items);
    ASSERT_FALSE(refreshedSecond.candidates->items->empty());
    EXPECT_NE(refreshedSecond.candidates->items->front().text, u"\u8A9E");
}

TEST(ImeEngineTest, PreviewSelectionPrefetchesNextSegmentCandidates) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    for (char16_t ch : std::u16string{u"kyouha"}) {
        engine.processKeyEvent(key(input::KeyCode::Unknown, ch));
    }

    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    auto converting = engine.processKeyEvent(shiftSpace);
    ASSERT_TRUE(converting.composition.has_value());
    ASSERT_TRUE(converting.candidates.has_value());
    ASSERT_TRUE(converting.candidates->items);
    ASSERT_FALSE(converting.candidates->items->empty());

    auto advanced = engine.processKeyEvent(key(input::KeyCode::Enter));
    ASSERT_TRUE(advanced.composition.has_value());
    ASSERT_TRUE(advanced.candidates.has_value());
    ASSERT_TRUE(advanced.candidates->items);
    ASSERT_FALSE(advanced.candidates->items->empty());
    EXPECT_EQ(advanced.composition->segmentStart, 2u);
    EXPECT_EQ(advanced.composition->segmentEnd, 3u);
}

TEST(ImeEngineTest, ShiftRightExpandsSelectedSegmentIntoNextSegment) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::W, u'w'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::T, u't'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::S, u's'));
    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    engine.processKeyEvent(key(input::KeyCode::I, u'i'));
    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::T, u't'));
    engine.processKeyEvent(key(input::KeyCode::E, u'e'));
    engine.processKeyEvent(key(input::KeyCode::N, u'n'));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::I, u'i'));

    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    engine.processKeyEvent(shiftSpace);
    const auto right = engine.processKeyEvent(key(input::KeyCode::Right));
    ASSERT_TRUE(right.composition.has_value());

    input::KeyEvent shiftRight(input::KeyCode::Right, std::nullopt, true, true, false, false);
    auto expanded = engine.processKeyEvent(shiftRight);

    ASSERT_TRUE(expanded.composition.has_value());
    EXPECT_EQ(expanded.composition->segmentStart, right.composition->segmentStart);
    EXPECT_TRUE(expanded.composition->segmentEnd > right.composition->segmentEnd);
    EXPECT_TRUE(expanded.composition->visible);
}

TEST(ImeEngineTest, ShiftLeftExpandsSelectedSegmentIntoPreviousSegment) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::W, u'w'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::T, u't'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::S, u's'));
    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    engine.processKeyEvent(key(input::KeyCode::I, u'i'));
    engine.processKeyEvent(key(input::KeyCode::H, u'h'));
    engine.processKeyEvent(key(input::KeyCode::A, u'a'));
    engine.processKeyEvent(key(input::KeyCode::T, u't'));
    engine.processKeyEvent(key(input::KeyCode::E, u'e'));
    engine.processKeyEvent(key(input::KeyCode::N, u'n'));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::I, u'i'));

    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    engine.processKeyEvent(shiftSpace);
    engine.processKeyEvent(key(input::KeyCode::Right));
    const auto selected = engine.processKeyEvent(key(input::KeyCode::Right));
    ASSERT_TRUE(selected.composition.has_value());
    input::KeyEvent shiftLeft(input::KeyCode::Left, std::nullopt, true, true, false, false);
    auto expanded = engine.processKeyEvent(shiftLeft);

    ASSERT_TRUE(expanded.composition.has_value());
    EXPECT_TRUE(expanded.composition->segmentStart < selected.composition->segmentStart);
    EXPECT_TRUE(expanded.composition->segmentEnd >= selected.composition->segmentEnd);
    EXPECT_TRUE(expanded.composition->visible);
}

TEST(ImeEngineTest, UpAndDownCycleCandidatesWithinSelectedSegment) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    auto converting = engine.processKeyEvent(shiftSpace);

    ASSERT_TRUE(converting.candidates.has_value());
    auto down = engine.processKeyEvent(key(input::KeyCode::Down));
    ASSERT_TRUE(down.candidates.has_value());
    EXPECT_EQ(down.candidates->selectedIndex, 1);

    auto up = engine.processKeyEvent(key(input::KeyCode::Up));
    ASSERT_TRUE(up.candidates.has_value());
    EXPECT_EQ(up.candidates->selectedIndex, 0);
}

TEST(ImeEngineTest, EndAndHomeJumpAcrossPredictionCandidates) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    auto opened = engine.processKeyEvent(key(input::KeyCode::Down));

    ASSERT_TRUE(opened.candidates.has_value());
    ASSERT_TRUE(opened.candidates->items);
    ASSERT_FALSE(opened.candidates->items->empty());

    auto end = engine.processKeyEvent(key(input::KeyCode::End));
    ASSERT_TRUE(end.candidates.has_value());
    ASSERT_TRUE(end.candidates->items);
    EXPECT_EQ(
        end.candidates->selectedIndex,
        static_cast<int32_t>(end.candidates->items->size()) - 1);

    auto home = engine.processKeyEvent(key(input::KeyCode::Home));
    ASSERT_TRUE(home.candidates.has_value());
    EXPECT_EQ(home.candidates->selectedIndex, 0);
}

TEST(ImeEngineTest, PageKeysClampPredictionSelection) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    auto opened = engine.processKeyEvent(key(input::KeyCode::Down));

    ASSERT_TRUE(opened.candidates.has_value());
    ASSERT_TRUE(opened.candidates->items);
    ASSERT_FALSE(opened.candidates->items->empty());

    auto pagedDown = engine.processKeyEvent(key(input::KeyCode::PageDown));
    ASSERT_TRUE(pagedDown.candidates.has_value());
    ASSERT_TRUE(pagedDown.candidates->items);
    EXPECT_EQ(
        pagedDown.candidates->selectedIndex,
        static_cast<int32_t>(pagedDown.candidates->items->size()) - 1);

    auto pagedUp = engine.processKeyEvent(key(input::KeyCode::PageUp));
    ASSERT_TRUE(pagedUp.candidates.has_value());
    EXPECT_EQ(pagedUp.candidates->selectedIndex, 0);
}

TEST(ImeEngineTest, NumberKeyCanCommitSingleSegmentCandidate) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    engine.processKeyEvent(shiftSpace);

    auto output = engine.processKeyEvent(key(input::KeyCode::Num1));

    EXPECT_EQ(output.nextState, state::ImeState::Committed);
    ASSERT_TRUE(output.commit.has_value());
    EXPECT_EQ(*output.commit, u"\u4ECA\u65E5");
}

TEST(ImeEngineTest, EndAndHomeJumpAcrossConversionCandidates) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    auto converting = engine.processKeyEvent(shiftSpace);

    ASSERT_TRUE(converting.candidates.has_value());
    ASSERT_TRUE(converting.candidates->items);
    ASSERT_FALSE(converting.candidates->items->empty());

    auto end = engine.processKeyEvent(key(input::KeyCode::End));
    ASSERT_TRUE(end.candidates.has_value());
    ASSERT_TRUE(end.candidates->items);
    EXPECT_EQ(
        end.candidates->selectedIndex,
        static_cast<int32_t>(end.candidates->items->size()) - 1);

    auto home = engine.processKeyEvent(key(input::KeyCode::Home));
    ASSERT_TRUE(home.candidates.has_value());
    EXPECT_EQ(home.candidates->selectedIndex, 0);
}

TEST(ImeEngineTest, SpaceMovesToNextConversionCandidateWithoutCommit) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    const auto converting = engine.processKeyEvent(shiftSpace);

    ASSERT_TRUE(converting.candidates.has_value());
    ASSERT_TRUE(converting.candidates->items);
    ASSERT_TRUE(converting.candidates->items->size() >= 2u);

    const auto spaced = engine.processKeyEvent(key(input::KeyCode::Space));

    EXPECT_EQ(spaced.nextState, state::ImeState::Converting);
    EXPECT_TRUE(spaced.isConsumed);
    EXPECT_FALSE(spaced.commit.has_value());
    ASSERT_TRUE(spaced.candidates.has_value());
    EXPECT_EQ(spaced.candidates->selectedIndex, 1);
}

TEST(ImeEngineTest, FinalizeCandidateSelectionCommitsPrediction) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    const auto visible = engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    ASSERT_TRUE(visible.candidates.has_value());
    ASSERT_TRUE(visible.candidates->items);
    ASSERT_FALSE(visible.candidates->items->empty());
    auto output = engine.finalizeCandidateSelection();

    ASSERT_TRUE(output.has_value());
    ASSERT_TRUE(output->commit.has_value());
    EXPECT_EQ(*output->commit, visible.candidates->items->front().text);
    EXPECT_EQ(output->nextState, state::ImeState::Committed);
}

TEST(ImeEngineTest, AbortCandidateSelectionClosesPredictionList) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    auto composing = engine.processKeyEvent(key(input::KeyCode::O, u'o'));

    ASSERT_TRUE(composing.candidates.has_value());
    auto output = engine.abortCandidateSelection();

    ASSERT_TRUE(output.has_value());
    EXPECT_EQ(output->nextState, state::ImeState::Composing);
    EXPECT_TRUE(output->shouldCloseCandidates());
    ASSERT_TRUE(output->composition.has_value());
    EXPECT_EQ(output->composition->text, u"\u304D\u3087");
}

TEST(ImeEngineTest, EscapeClosesPredictionListDuringComposition) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    auto composing = engine.processKeyEvent(key(input::KeyCode::O, u'o'));

    ASSERT_TRUE(composing.candidates.has_value());
    auto output = engine.processKeyEvent(key(input::KeyCode::Escape));

    EXPECT_EQ(output.nextState, state::ImeState::Composing);
    EXPECT_TRUE(output.isConsumed);
    EXPECT_TRUE(output.shouldCloseCandidates());
    ASSERT_TRUE(output.composition.has_value());
    EXPECT_EQ(output.composition->text, u"\u304D\u3087");
}

TEST(ImeEngineTest, EscapeCancelsConversionBackToComposing) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    engine.processKeyEvent(shiftSpace);

    const auto output = engine.processKeyEvent(key(input::KeyCode::Escape));

    EXPECT_EQ(output.nextState, state::ImeState::Composing);
    EXPECT_TRUE(output.isConsumed);
    EXPECT_TRUE(output.shouldCloseCandidates());
    ASSERT_TRUE(output.composition.has_value());
    EXPECT_EQ(output.composition->text, u"\u304D\u3087\u3046");
}

TEST(ImeEngineTest, BackspaceCancelsConversionBackToComposing) {
    auto engine = makeEngine();
    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    engine.processKeyEvent(shiftSpace);

    const auto output = engine.processKeyEvent(key(input::KeyCode::Backspace));

    EXPECT_EQ(output.nextState, state::ImeState::Composing);
    EXPECT_TRUE(output.isConsumed);
    EXPECT_TRUE(output.shouldCloseCandidates());
    ASSERT_TRUE(output.composition.has_value());
    EXPECT_EQ(output.composition->text, u"\u304D\u3087\u3046");
}

TEST(ImeEngineTest, LearningPromotesCommittedCandidate) {
    auto engine = makeEngine();

    for (int i = 0; i < 2; ++i) {
        engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
        engine.processKeyEvent(key(input::KeyCode::K, u'k'));
        engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
        engine.processKeyEvent(key(input::KeyCode::O, u'o'));
        engine.processKeyEvent(key(input::KeyCode::U, u'u'));
        input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
        engine.processKeyEvent(shiftSpace);
        engine.processKeyEvent(key(input::KeyCode::Down));
        engine.processKeyEvent(key(input::KeyCode::Enter));
    }

    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    input::KeyEvent shiftSpace(input::KeyCode::Space, std::nullopt, true, true, false, false);
    auto converting = engine.processKeyEvent(shiftSpace);

    ASSERT_TRUE(converting.candidates.has_value());
    ASSERT_TRUE(converting.candidates->items);
    ASSERT_FALSE(converting.candidates->items->empty());
    EXPECT_EQ(converting.candidates->items->front().text, u"\u5F37");
}

TEST(DictionaryTest, UserDbLearningPromotesPrediction) {
    dictionary::Dictionary dictionary;
    dictionary.recordCommit(u"\u306B\u307B\u3093\u3054", u"\u65E5\u672C\u8A9E");

    auto predictions = dictionary.getPredictionsShared(u"\u306B");

    ASSERT_TRUE(predictions);
    ASSERT_FALSE(predictions->empty());
    EXPECT_EQ(predictions->front().text, u"\u65E5\u672C\u8A9E");
}

TEST(DictionaryTest, FolderNameProvidesPartOfSpeechMetadata) {
    dictionary::Dictionary dictionary;

    EXPECT_EQ(
        dictionary.lookupPartOfSpeech(u"\u306B", u"\u4E8C"),
        dictionary::PartOfSpeech::Noun);
    EXPECT_EQ(
        dictionary.lookupPartOfSpeech(u"\u3092", u"\u3092"),
        dictionary::PartOfSpeech::Particle);
    EXPECT_EQ(
        dictionary.lookupPartOfSpeech(u"\u304B\u3044", u"\u8CB7\u3044"),
        dictionary::PartOfSpeech::Verb);
    EXPECT_EQ(
        dictionary.lookupPartOfSpeech(u"\u3067\u3059", u"\u3067\u3059"),
        dictionary::PartOfSpeech::Copula);
    EXPECT_EQ(
        dictionary.lookupPartOfSpeech(u"\u307E\u3059", u"\u307E\u3059"),
        dictionary::PartOfSpeech::Ending);
    EXPECT_EQ(
        dictionary.lookupPartOfSpeech(u"\u3067\u3057\u3087", u"\u3067\u3057\u3087\u3046"),
        dictionary::PartOfSpeech::Modal);
}

TEST(DictionaryTest, CurrencyCodesAreCappedAroundEighty) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u3086\u30FC\u3048\u3059\u3067\u3043\u30FC",
        dictionary::PartOfSpeech::Unknown);

    ASSERT_TRUE(candidates);
    const auto codeIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"USD"; });
    ASSERT_TRUE(codeIt != candidates->end());
    EXPECT_TRUE(codeIt->score <= 80);
}

TEST(DictionaryTest, NounContextPromotesParticleCandidate) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u306B",
        dictionary::PartOfSpeech::Noun);

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u306B");
}

TEST(DictionaryTest, ParticleContextPromotesNounCandidate) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u304B\u3044",
        dictionary::PartOfSpeech::Particle);

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u4F1A");
}

TEST(DictionaryTest, ObjectParticleSurfacePromotesVerbCandidate) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u304B\u3044",
        dictionary::PartOfSpeech::Particle,
        u"\u3092");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u8CB7\u3044");
}

TEST(DictionaryTest, ObjectParticleSurfacePromotesVerbPrediction) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getPredictionsShared(
        u"\u304B",
        8,
        dictionary::PartOfSpeech::Particle,
        u"\u3092");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    const auto buyIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u8CB7\u3044"; });
    const auto meetingIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u4F1A"; });

    ASSERT_TRUE(buyIt != candidates->end());
    ASSERT_TRUE(meetingIt != candidates->end());
    EXPECT_TRUE(buyIt->score > meetingIt->score);
}

TEST(DictionaryTest, GenitiveSurfacePromotesNounCandidate) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u306B",
        dictionary::PartOfSpeech::Particle,
        u"\u306E");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u4E8C");
}

TEST(DictionaryTest, GenitiveSurfacePromotesNounPrediction) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getPredictionsShared(
        u"\u306B",
        8,
        dictionary::PartOfSpeech::Particle,
        u"\u306E");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    const auto nounIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u4E8C"; });
    const auto particleIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u306B"; });

    ASSERT_TRUE(nounIt != candidates->end());
    ASSERT_TRUE(particleIt != candidates->end());
    EXPECT_TRUE(nounIt->score > particleIt->score);
}

TEST(DictionaryTest, TopicSurfacePromotesPredicateCandidate) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u304b\u3044",
        dictionary::PartOfSpeech::Particle,
        u"\u306f");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u8cb7\u3044");
}

TEST(DictionaryTest, TopicSurfacePromotesPredicatePrediction) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getPredictionsShared(
        u"\u304b",
        8,
        dictionary::PartOfSpeech::Particle,
        u"\u306f");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    const auto buyIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u8cb7\u3044"; });
    const auto meetingIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u4f1a"; });

    ASSERT_TRUE(buyIt != candidates->end());
    ASSERT_TRUE(meetingIt != candidates->end());
    EXPECT_TRUE(buyIt->score > meetingIt->score);
}

TEST(DictionaryTest, TopicSurfaceDemotesRawAndRomajiPredictions) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getPredictionsShared(
        u"\u304b",
        8,
        dictionary::PartOfSpeech::Particle,
        u"\u306f");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    const auto buyIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u8CB7\u3044"; });
    const auto rawIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u304b"; });
    const auto romajiIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"ka"; });

    ASSERT_TRUE(buyIt != candidates->end());
    ASSERT_TRUE(rawIt != candidates->end());
    ASSERT_TRUE(romajiIt != candidates->end());
    EXPECT_TRUE(buyIt->score > rawIt->score);
    EXPECT_TRUE(buyIt->score > romajiIt->score);
}

TEST(DictionaryTest, CompoundParticleDehaPromotesPredicateCandidate) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u304b\u3044",
        dictionary::PartOfSpeech::Particle,
        u"\u3067\u306f");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u8cb7\u3044");
}

TEST(DictionaryTest, CompoundParticleDehaPromotesPredicatePrediction) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getPredictionsShared(
        u"\u304b",
        8,
        dictionary::PartOfSpeech::Particle,
        u"\u3067\u306f");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    const auto buyIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u8CB7\u3044"; });
    const auto meetingIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u4F1A"; });

    ASSERT_TRUE(buyIt != candidates->end());
    ASSERT_TRUE(meetingIt != candidates->end());
    EXPECT_TRUE(buyIt->score > meetingIt->score);
}

TEST(DictionaryTest, ClauseParticleNodePromotesPredicateCandidate) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u304b\u3044",
        dictionary::PartOfSpeech::Particle,
        u"\u306e\u3067");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u8cb7\u3044");
}

TEST(DictionaryTest, LearnedSurfaceContextPromotesMatchingNextCandidate) {
    dictionary::Dictionary dictionary;

    dictionary.recordCommit(u"\u306B\u307B\u3093", u"\u65E5\u672C");
    dictionary.recordCommit(u"\u3054", u"\u8A9E");
    dictionary.recordCommit(u"\u306B\u307B\u3093", u"\u65E5\u672C");

    const auto candidates = dictionary.getCandidatesShared(
        u"\u3054",
        dictionary::PartOfSpeech::Noun,
        u"\u65E5\u672C");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u8A9E");
}

TEST(DictionaryTest, PlaceNamesAreDemotedWithoutContext) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u3072\u304B\u308A",
        dictionary::PartOfSpeech::Unknown);

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u5149");
}

TEST(DictionaryTest, PlaceNamesStayLowPriorityAfterParticles) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u3072\u304B\u308A",
        dictionary::PartOfSpeech::Particle);

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u5149");
    const auto placeIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) { return candidate.text == u"\u5149\u5E02"; });
    ASSERT_TRUE(placeIt != candidates->end());
    EXPECT_TRUE(placeIt->score <= 40 + 180);
}

TEST(DictionaryTest, VerbContextPromotesEndingCandidate) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u307E\u3059",
        dictionary::PartOfSpeech::Verb);

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u307E\u3059");
}

TEST(DictionaryTest, EndingContextPromotesModalCandidate) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(
        u"\u3067\u3057\u3087",
        dictionary::PartOfSpeech::Ending);

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u3067\u3057\u3087\u3046");
}

TEST(DictionaryTest, DuplicateReadingAndSurfaceUseHighestScoreOnlyOnce) {
    dictionary::Dictionary dictionary;
    const auto before = dictionary.getCandidatesShared(u"\u306B\u307B\u3093\u3054");

    ASSERT_TRUE(before);
    const auto beforeIt = std::find_if(
        before->begin(),
        before->end(),
        [](const auto& candidate) {
            return candidate.reading == u"\u306B\u307B\u3093\u3054" &&
                   candidate.text == u"\u65E5\u672C\u8A9E";
        });
    ASSERT_TRUE(beforeIt != before->end());

    dictionary.recordCommit(u"\u306B\u307B\u3093\u3054", u"\u65E5\u672C\u8A9E");
    const auto after = dictionary.getCandidatesShared(u"\u306B\u307B\u3093\u3054");

    ASSERT_TRUE(after);
    int duplicateCount = 0;
    std::optional<int32_t> mergedScore;
    for (const auto& candidate : *after) {
        if (candidate.reading == u"\u306B\u307B\u3093\u3054" &&
            candidate.text == u"\u65E5\u672C\u8A9E") {
            ++duplicateCount;
            mergedScore = candidate.score;
        }
    }

    EXPECT_EQ(duplicateCount, 1);
    ASSERT_TRUE(mergedScore.has_value());
    EXPECT_TRUE(*mergedScore > beforeIt->score);
}

TEST(DictionaryTest, ExactCandidatesAppendRomajiAboveKatakanaAtBottom) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(u"\u304D\u3087\u3046");

    ASSERT_TRUE(candidates);
    const auto romajiIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) {
            return candidate.text == u"kyou" && candidate.reading == u"\u304D\u3087\u3046";
        });
    const auto katakanaIt = std::find_if(
        candidates->begin(),
        candidates->end(),
        [](const auto& candidate) {
            return candidate.text == u"\u30AD\u30E7\u30A6" && candidate.reading == u"\u304D\u3087\u3046";
        });

    ASSERT_TRUE(romajiIt != candidates->end());
    ASSERT_TRUE(katakanaIt != candidates->end());
}

TEST(DictionaryTest, NumericReadingsAppendSyntheticFormattingCandidates) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(u"1234");

    ASSERT_TRUE(candidates);
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"1234" && candidate.text == u"\uFF11\uFF12\uFF13\uFF14";
        }),
        candidates->end());
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"1234" && candidate.text == u"1,234";
        }),
        candidates->end());
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"1234" && candidate.text == u"\u4E00\u4E8C\u4E09\u56DB";
        }),
        candidates->end());
}

TEST(DictionaryTest, CompactDateReadingsAppendDateCandidates) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(u"20260411");

    ASSERT_TRUE(candidates);
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"20260411" && candidate.text == u"2026/04/11";
        }),
        candidates->end());
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"20260411" && candidate.text == u"2026\u5E744\u670811\u65E5";
        }),
        candidates->end());
}

TEST(DictionaryTest, CompactDatePredictionsAppendDateCandidates) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getPredictionsShared(u"20260411");

    ASSERT_TRUE(candidates);
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"20260411" && candidate.text == u"2026/04/11";
        }),
        candidates->end());
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"20260411" && candidate.text == u"2026\u5E744\u670811\u65E5";
        }),
        candidates->end());
}

TEST(DictionaryTest, CompactYearMonthReadingsAppendYearMonthCandidates) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(u"202604");

    ASSERT_TRUE(candidates);
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"202604" && candidate.text == u"2026/04";
        }),
        candidates->end());
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"202604" && candidate.text == u"2026\u5E744\u6708";
        }),
        candidates->end());
}

TEST(DictionaryTest, NumericCurrencyReadingsAppendYenCandidates) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(u"1234\u3048\u3093");

    ASSERT_TRUE(candidates);
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"1234\u3048\u3093" && candidate.text == u"1,234\u5186";
        }),
        candidates->end());
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"1234\u3048\u3093" && candidate.text == u"\uFF11\uFF12\uFF13\uFF14\u5186";
        }),
        candidates->end());
}

TEST(DictionaryTest, NumericTimeReadingsAppendHourCandidates) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getPredictionsShared(u"3\u3058");

    ASSERT_TRUE(candidates);
    EXPECT_NE(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.reading == u"3\u3058" && candidate.text == u"3\u6642";
        }),
        candidates->end());
}

TEST(DictionaryTest, InvalidCompactDateDoesNotAppendDateCandidates) {
    dictionary::Dictionary dictionary;

    const auto candidates = dictionary.getCandidatesShared(u"20260230");

    ASSERT_TRUE(candidates);
    EXPECT_EQ(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.text == u"2026/02/30";
        }),
        candidates->end());
    EXPECT_EQ(
        std::find_if(candidates->begin(), candidates->end(), [](const auto& candidate) {
            return candidate.text == u"2026\u5E742\u670830\u65E5";
        }),
        candidates->end());
}

TEST(DictionaryTest, SyntheticRomajiAndKatakanaAreSkippedWhenDictionaryAlreadyHasThem) {
    dictionary::Dictionary dictionary;
    dictionary.recordCommit(u"\u304D\u3087\u3046", u"kyou");
    dictionary.recordCommit(u"\u304D\u3087\u3046", u"\u30AD\u30E7\u30A6");

    const auto candidates = dictionary.getCandidatesShared(u"\u304D\u3087\u3046");

    ASSERT_TRUE(candidates);
    int romajiCount = 0;
    int katakanaCount = 0;
    for (const auto& candidate : *candidates) {
        if (candidate.reading != u"\u304D\u3087\u3046") {
            continue;
        }
        if (candidate.text == u"kyou") {
            ++romajiCount;
        }
        if (candidate.text == u"\u30AD\u30E7\u30A6") {
            ++katakanaCount;
        }
    }

    EXPECT_EQ(romajiCount, 1);
    EXPECT_EQ(katakanaCount, 1);
}

TEST(DictionaryTest, BlackDbRemovesBlockedCandidate) {
    dictionary::Dictionary dictionary;
    dictionary.blockCandidate(u"\u304D\u3087\u3046", u"\u4ECA\u65E5");

    auto candidates = dictionary.getCandidatesShared(u"\u304D\u3087\u3046");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_NE(candidates->front().text, u"\u4ECA\u65E5");
    for (const auto& candidate : *candidates) {
        EXPECT_NE(candidate.text, u"\u4ECA\u65E5");
    }
}

TEST(DictionaryTest, BlackDbAffectsExactReadingDetection) {
    dictionary::Dictionary dictionary;
    dictionary.blockCandidate(u"\u308F\u305F\u3057", u"\u79C1");
    dictionary.blockCandidate(u"\u308F\u305F\u3057", u"\u6E21\u3057");

    EXPECT_FALSE(dictionary.hasExactReading(u"\u308F\u305F\u3057"));
}

TEST(DictionaryTest, EmptyCommitsAndBlocksAreIgnored) {
    const auto tempDir = makeDictionaryTempDir("ignore_empty_entries");
    dictionary::DictionaryStoragePaths paths;
    paths.userDbPath = tempDir / "user_db.bin";
    paths.blackDbPath = tempDir / "black_db.bin";
    dictionary::Dictionary dictionary(paths);

    dictionary.recordCommit(u"", u"\u65E5\u672C\u8A9E");
    dictionary.recordCommit(u"\u3066\u3059\u3068\u3088\u307F", u"");
    dictionary.blockCandidate(u"", u"\u4ECA\u65E5");
    dictionary.blockCandidate(u"\u3058\u3055\u304F", u"");

    auto predictions = dictionary.getPredictionsShared(u"\u3066");
    ASSERT_TRUE(predictions);
    for (const auto& candidate : *predictions) {
        EXPECT_NE(candidate.text, u"\u65E5\u672C\u8A9E");
    }

    auto candidates = dictionary.getCandidatesShared(u"\u3058\u3055\u304F");
    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    bool foundRawCandidate = false;
    for (const auto& candidate : *candidates) {
        if (candidate.text == u"\u3058\u3055\u304F") {
            foundRawCandidate = true;
            break;
        }
    }
    EXPECT_TRUE(foundRawCandidate);
}

TEST(DictionaryTest, UserAndBlackDbPersistAcrossReload) {
    namespace fs = std::filesystem;
    const fs::path tempDir = fs::path("build") / "gtest_db";
    const auto userPath = tempDir / "user_db.bin";
    const auto blackPath = tempDir / "black_db.bin";
    std::error_code ec;
    fs::remove(userPath, ec);
    fs::remove(blackPath, ec);
    fs::remove_all(tempDir, ec);

    dictionary::DictionaryStoragePaths paths;
    paths.userDbPath = userPath;
    paths.blackDbPath = blackPath;

    {
        dictionary::Dictionary dictionary(paths);
        dictionary.recordCommit(u"\u306B\u307B\u3093\u3054", u"\u65E5\u672C\u8A9E");
        dictionary.blockCandidate(u"\u304D\u3087\u3046", u"\u4ECA\u65E5");
    }

    dictionary::Dictionary reloaded(paths);
    auto predictions = reloaded.getPredictionsShared(u"\u306B");
    ASSERT_TRUE(predictions);
    ASSERT_FALSE(predictions->empty());
    EXPECT_NE(
        std::find_if(
            predictions->begin(),
            predictions->end(),
            [](const auto& candidate) { return candidate.text == u"\u65E5\u672C\u8A9E"; }),
        predictions->end());

    auto candidates = reloaded.getCandidatesShared(u"\u304D\u3087\u3046");
    ASSERT_TRUE(candidates);
    for (const auto& candidate : *candidates) {
        EXPECT_NE(candidate.text, u"\u4ECA\u65E5");
    }

    fs::remove(userPath, ec);
    fs::remove(blackPath, ec);
    fs::remove_all(tempDir, ec);
}

TEST(DictionaryTest, ReloadCreatesMissingUserAndBlackDbFiles) {
    namespace fs = std::filesystem;
    const fs::path tempDir = makeDictionaryTempDir("create_missing_dbs");
    const auto userPath = tempDir / "user_db.bin";
    const auto blackPath = tempDir / "black_db.bin";
    std::error_code ec;
    fs::remove(userPath, ec);
    fs::remove(blackPath, ec);

    dictionary::DictionaryStoragePaths paths;
    paths.userDbPath = userPath;
    paths.blackDbPath = blackPath;

    dictionary::Dictionary dictionary(paths);

    EXPECT_TRUE(fs::exists(userPath));
    EXPECT_TRUE(fs::exists(blackPath));
}

TEST(DictionaryTest, LearnedSurfaceContextPersistsAcrossReload) {
    namespace fs = std::filesystem;
    const fs::path tempDir = makeDictionaryTempDir("context_persist");
    dictionary::DictionaryStoragePaths paths;
    paths.userDbPath = tempDir / "user_db.bin";
    paths.blackDbPath = tempDir / "black_db.bin";

    {
        dictionary::Dictionary dictionary(paths);
        dictionary.recordCommit(u"\u306B\u307B\u3093", u"\u65E5\u672C");
        dictionary.recordCommit(u"\u3054", u"\u8A9E");
        dictionary.recordCommit(u"\u306B\u307B\u3093", u"\u65E5\u672C");
        ASSERT_TRUE(dictionary.flush());
    }

    dictionary::Dictionary reloaded(paths);
    const auto candidates = reloaded.getCandidatesShared(
        u"\u3054",
        dictionary::PartOfSpeech::Noun,
        u"\u65E5\u672C");

    ASSERT_TRUE(candidates);
    ASSERT_FALSE(candidates->empty());
    EXPECT_EQ(candidates->front().text, u"\u8A9E");
}

TEST(DictionaryTest, ReloadQuarantinesEmptyUserDbFile) {
    namespace fs = std::filesystem;
    const fs::path tempDir = makeDictionaryTempDir("empty_user");
    const auto userPath = tempDir / "user_db.bin";
    const auto blackPath = tempDir / "black_db.bin";
    writeBytes(userPath, {});

    dictionary::DictionaryStoragePaths paths;
    paths.userDbPath = userPath;
    paths.blackDbPath = blackPath;

    dictionary::Dictionary dictionary(paths);

    EXPECT_FALSE(fs::exists(userPath));
    const auto quarantined = findCorruptBackups(tempDir, "user_db.bin");
    EXPECT_EQ(quarantined.size(), 1u);

    auto predictions = dictionary.getPredictionsShared(u"\u306B");
    ASSERT_TRUE(predictions);
    ASSERT_FALSE(predictions->empty());
}

TEST(DictionaryTest, ReloadQuarantinesInvalidBlackDbHeader) {
    namespace fs = std::filesystem;
    const fs::path tempDir = makeDictionaryTempDir("invalid_black_header");
    const auto userPath = tempDir / "user_db.bin";
    const auto blackPath = tempDir / "black_db.bin";
    writeBytes(blackPath, {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00});

    dictionary::DictionaryStoragePaths paths;
    paths.userDbPath = userPath;
    paths.blackDbPath = blackPath;

    dictionary::Dictionary dictionary(paths);

    EXPECT_FALSE(fs::exists(blackPath));
    const auto quarantined = findCorruptBackups(tempDir, "black_db.bin");
    EXPECT_EQ(quarantined.size(), 1u);
}

TEST(DictionaryTest, BlackDbRejectsOversizedEntryOnSave) {
    namespace fs = std::filesystem;
    const fs::path tempDir = makeDictionaryTempDir("oversized_black_entry");
    const auto blackPath = tempDir / "black_db.bin";

    dictionary::BlackDb blackDb;
    blackDb.block(std::u16string(65537, u'a'), u"\u4ECA\u65E5");

    EXPECT_FALSE(blackDb.saveToFile(blackPath));
    EXPECT_FALSE(fs::exists(blackPath));
    EXPECT_FALSE(fs::exists(blackPath.string() + ".tmp"));
}

TEST(DictionaryTest, UserDbRejectsOversizedEntryOnSave) {
    namespace fs = std::filesystem;
    const fs::path tempDir = makeDictionaryTempDir("oversized_user_entry");
    const auto userPath = tempDir / "user_db.bin";

    dictionary::UserDb userDb;
    userDb.recordCommit(std::u16string(65537, u'a'), u"\u4ECA\u65E5");

    EXPECT_FALSE(userDb.saveToFile(userPath));
    EXPECT_FALSE(fs::exists(userPath));
    EXPECT_FALSE(fs::exists(userPath.string() + ".tmp"));
}

TEST(DictionaryTest, UserDbDiscardPartialEntriesAfterFailedLoad) {
    namespace fs = std::filesystem;
    const fs::path tempDir = makeDictionaryTempDir("partial_user_load");
    const auto userPath = tempDir / "user_db.bin";
    writeBytes(
        userPath,
        {
            0x31, 0x42, 0x44, 0x55,
            0x02, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x61, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x41, 0x00,
            0x01, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00
        });

    dictionary::UserDb userDb;
    userDb.recordCommit(u"seed", u"seed");
    EXPECT_FALSE(userDb.loadFromFile(userPath));
    EXPECT_EQ(userDb.findExact(u"a"), nullptr);
    EXPECT_FALSE(userDb.hasExactReading(u"a"));
    EXPECT_EQ(userDb.findExact(u"seed"), nullptr);
}

TEST(DictionaryTest, BlackDbDiscardPartialEntriesAfterFailedLoad) {
    namespace fs = std::filesystem;
    const fs::path tempDir = makeDictionaryTempDir("partial_black_load");
    const auto blackPath = tempDir / "black_db.bin";
    writeBytes(
        blackPath,
        {
            0x31, 0x42, 0x44, 0x42,
            0x02, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x61, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x41, 0x00,
            0x01, 0x00, 0x00, 0x00
        });

    dictionary::BlackDb blackDb;
    blackDb.block(u"seed", u"seed");
    EXPECT_FALSE(blackDb.loadFromFile(blackPath));
    EXPECT_FALSE(blackDb.isBlocked(u"a", u"A"));
    EXPECT_FALSE(blackDb.isBlocked(u"seed", u"seed"));
}

TEST(DictionaryTest, ReloadQuarantinesTruncatedUserDbEntry) {
    namespace fs = std::filesystem;
    const fs::path tempDir = makeDictionaryTempDir("truncated_user_entry");
    const auto userPath = tempDir / "user_db.bin";
    const auto blackPath = tempDir / "black_db.bin";
    writeBytes(
        userPath,
        {
            0x31, 0x42, 0x44, 0x55,
            0x01, 0x00, 0x00, 0x00,
            0x02, 0x00, 0x00, 0x00,
            0x42, 0x30
        });

    dictionary::DictionaryStoragePaths paths;
    paths.userDbPath = userPath;
    paths.blackDbPath = blackPath;

    dictionary::Dictionary dictionary(paths);

    EXPECT_FALSE(fs::exists(userPath));
    const auto quarantined = findCorruptBackups(tempDir, "user_db.bin");
    EXPECT_EQ(quarantined.size(), 1u);
}

TEST(DictionaryTest, FlushUsesTemporaryFilesAndLeavesNoTmpArtifacts) {
    namespace fs = std::filesystem;
    const fs::path tempDir = makeDictionaryTempDir("atomic_flush");
    const auto userPath = tempDir / "user_db.bin";
    const auto blackPath = tempDir / "black_db.bin";

    dictionary::DictionaryStoragePaths paths;
    paths.userDbPath = userPath;
    paths.blackDbPath = blackPath;

    dictionary::Dictionary dictionary(paths);
    dictionary.recordCommit(u"\u306B\u307B\u3093\u3054", u"\u65E5\u672C\u8A9E");
    dictionary.blockCandidate(u"\u304D\u3087\u3046", u"\u4ECA\u65E5");

    ASSERT_TRUE(dictionary.flush());
    EXPECT_TRUE(fs::exists(userPath));
    EXPECT_TRUE(fs::exists(blackPath));
    EXPECT_FALSE(fs::exists(userPath.string() + ".tmp"));
    EXPECT_FALSE(fs::exists(blackPath.string() + ".tmp"));

    dictionary::Dictionary reloaded(paths);
    auto predictions = reloaded.getPredictionsShared(u"\u306B");
    ASSERT_TRUE(predictions);
    ASSERT_FALSE(predictions->empty());
    EXPECT_NE(
        std::find_if(
            predictions->begin(),
            predictions->end(),
            [](const auto& candidate) { return candidate.text == u"\u65E5\u672C\u8A9E"; }),
        predictions->end());
}

TEST(DictionaryTest, DefaultStoragePathsUseModuleDataDirectory) {
    dictionary::DictionaryStoragePaths paths;

    EXPECT_EQ(paths.userDbPath, yume::utils::paths::dataPath("dictionary/db/user_db.bin"));
    EXPECT_EQ(paths.blackDbPath, yume::utils::paths::dataPath("dictionary/db/black_db.bin"));
}

TEST(CandidateListTest, SanitizeSelectionClampsOutOfRangeIndex) {
    engine::CandidateList candidates;
    candidates.items = std::make_shared<const std::vector<engine::Candidate>>(
        std::vector<engine::Candidate>{
            {u"\u4ECA\u65E5", u"\u304D\u3087\u3046", 10},
            {u"\u5F37", u"\u304D\u3087\u3046", 5},
        });
    candidates.selectedIndex = 99;

    ASSERT_TRUE(candidates.sanitizeSelection());
    ASSERT_TRUE(candidates.selectedText() != nullptr);
    EXPECT_EQ(*candidates.selectedText(), u"\u5F37");
}

TEST(ImeEngineTest, PunctuationCommitDoesNotPolluteLearningSurface) {
    const auto paths = makeEngineStoragePaths();
    engine::ImeEngine engine(paths);

    engine.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    engine.processKeyEvent(key(input::KeyCode::K, u'k'));
    engine.processKeyEvent(key(input::KeyCode::Y, u'y'));
    engine.processKeyEvent(key(input::KeyCode::O, u'o'));
    const auto visible = engine.processKeyEvent(key(input::KeyCode::U, u'u'));
    ASSERT_TRUE(visible.candidates.has_value());
    ASSERT_TRUE(visible.candidates->items);
    ASSERT_FALSE(visible.candidates->items->empty());
    auto punctuated = engine.processKeyEvent(key(input::KeyCode::Unknown, u','));

    ASSERT_TRUE(punctuated.commit.has_value());
    EXPECT_EQ(*punctuated.commit, visible.candidates->items->front().text + u"\u3001");

    engine::ImeEngine requery(paths);
    requery.processKeyEvent(key(input::KeyCode::Space, std::nullopt, true));
    requery.processKeyEvent(key(input::KeyCode::K, u'k'));
    requery.processKeyEvent(key(input::KeyCode::Y, u'y'));
    requery.processKeyEvent(key(input::KeyCode::O, u'o'));
    requery.processKeyEvent(key(input::KeyCode::U, u'u'));
    auto opened = requery.processKeyEvent(key(input::KeyCode::Down));

    ASSERT_TRUE(opened.candidates.has_value());
    ASSERT_TRUE(opened.candidates->items);
    ASSERT_FALSE(opened.candidates->items->empty());
    EXPECT_NE(opened.candidates->items->front().text, u"\u304D\u3087\u3046\u3001");
}
