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



#include "ime/engine/ime_engine.hpp"

#include "utils/logger.hpp"

#include <stdexcept>

namespace yume::ime::engine {

namespace {

std::optional<SurfaceText> selectedCandidateText(const CandidateList& candidates) {
    if (const SurfaceText* text = candidates.selectedText()) {
        return *text;
    }
    return std::nullopt;
}

constexpr size_t kMaxRomajiUnitLength = 4;

}
ImeEngine::SessionSnapshot ImeEngine::captureSessionSnapshot() const {
    SessionSnapshot snapshot;
    snapshot.currentState = currentState;
    snapshot.buffer = buffer;
    snapshot.dictionary = dictionary.captureSessionSnapshot();
    snapshot.currentCandidates = currentCandidates;
    snapshot.isCompositionCandidateSelectionPrimed = isCompositionCandidateSelectionPrimed;
    snapshot.cachedConversion = cachedConversion;
    snapshot.currentSegments = currentSegments;
    snapshot.segmentCandidates = segmentCandidates;
    snapshot.cachedSegmentDisplayText = cachedSegmentDisplayText;
    snapshot.cachedSegmentDisplayRanges = cachedSegmentDisplayRanges;
    snapshot.isSegmentDisplayCacheDirty = isSegmentDisplayCacheDirty;
    snapshot.selectedSegmentIndex = selectedSegmentIndex;
    return snapshot;
}

void ImeEngine::restoreSessionSnapshot(const SessionSnapshot& snapshot) {
    currentState = snapshot.currentState;
    buffer = snapshot.buffer;
    dictionary.restoreSessionSnapshot(snapshot.dictionary);
    currentCandidates = snapshot.currentCandidates;
    isCompositionCandidateSelectionPrimed = snapshot.isCompositionCandidateSelectionPrimed;
    cachedConversion = snapshot.cachedConversion;
    currentSegments = snapshot.currentSegments;
    segmentCandidates = snapshot.segmentCandidates;
    cachedSegmentDisplayText = snapshot.cachedSegmentDisplayText;
    cachedSegmentDisplayRanges = snapshot.cachedSegmentDisplayRanges;
    isSegmentDisplayCacheDirty = snapshot.isSegmentDisplayCacheDirty;
    selectedSegmentIndex = snapshot.selectedSegmentIndex;
}

void ImeEngine::discardActiveSession() {
    resetImeSession();
    if (currentState != state::ImeState::Direct) {
        currentState = state::ImeState::Idle;
    }
}


void ImeEngine::serviceIdle() {
    dictionary.servicePersistence();
}

bool ImeEngine::hasActiveSession() const {
    return buffer.getLength() > 0 ||
           !currentSegments.empty() ||
           (currentCandidates.items && !currentCandidates.items->empty());
}

bool ImeEngine::hasVisibleCandidateWindow() const {
    return currentCandidates.items && !currentCandidates.items->empty();
}

std::optional<EngineOutput> ImeEngine::setCandidateSelection(CandidateIndex index) {
    if (!currentCandidates.items || currentCandidates.items->empty()) {
        return std::nullopt;
    }
    if (index < 0 || index >= static_cast<CandidateIndex>(currentCandidates.items->size())) {
        return std::nullopt;
    }

    currentCandidates.selectedIndex = index;
    if (currentState == state::ImeState::Converting) {
        persistCurrentCandidateSelection();
        previewSelectedCandidate();
        return createOutput(currentState, true);
    }

    if (currentState == state::ImeState::Composing) {
        isCompositionCandidateSelectionPrimed = true;
        return createOutput(currentState, true, &getOrBuildConversion());
    }

    return std::nullopt;
}

std::optional<EngineOutput> ImeEngine::finalizeCandidateSelection() {
    if (!currentCandidates.items || currentCandidates.items->empty()) {
        return std::nullopt;
    }

    if (currentState == state::ImeState::Composing) {
        EngineOutput output;
        output.nextState = state::ImeState::Committed;
        output.inputMode = InputMode::Hiragana;
        output.isConsumed = true;
        output.commit = tryGetSelectedCandidateText().value_or(
            buildReadingText(&getOrBuildConversion()).value_or(ReadingText{}));
        output.commitAction = CommitAction::Commit;

        const auto reading = buildReadingText(&getOrBuildConversion()).value_or(ReadingText{});
        dictionary.recordCommit(reading, *output.commit);
        resetImeSession();
        currentState = state::ImeState::Committed;
        return output;
    }

    if (currentState == state::ImeState::Converting) {
        persistCurrentCandidateSelection();
        previewSelectedCandidate();
        if (advanceSegmentAfterSelection()) {
            return createOutput(currentState, true);
        }

        EngineOutput output;
        output.nextState = state::ImeState::Committed;
        output.inputMode = InputMode::Hiragana;
        output.isConsumed = true;
        output.commit = buildCommittedText();
        output.commitAction = CommitAction::Commit;

        for (const auto& segment : currentSegments) {
            dictionary.recordCommit(segment.reading, segment.text);
        }

        resetImeSession();
        currentState = state::ImeState::Committed;
        return output;
    }

    return std::nullopt;
}

std::optional<EngineOutput> ImeEngine::abortCandidateSelection() {
    if (currentState == state::ImeState::Composing) {
        clearCandidates();
        return createOutput(currentState, true, &getOrBuildConversion());
    }

    if (currentState == state::ImeState::Converting) {
        currentState = state::ImeState::Composing;
        clearCandidates();
        currentSegments.clear();
        selectedSegmentIndex = 0;
        refreshPredictionsForComposition(&getOrBuildConversion());
        return createOutput(currentState, true, &getOrBuildConversion());
    }

    return std::nullopt;
}

EngineOutput ImeEngine::createOutput(
    state::ImeState nextState,
    bool isConsumed,
    const composition::Converter::ConversionResult* conversion) {
    EngineOutput output;
    output.nextState = nextState;
    output.isConsumed = isConsumed;
    output.inputMode = (nextState == state::ImeState::Direct) ? InputMode::Latin : InputMode::Hiragana;

    if (nextState == state::ImeState::Composing || nextState == state::ImeState::Converting) {
        output.compositionAction = CompositionAction::Update;
        populateComposition(output, conversion);
        if ((nextState == state::ImeState::Converting) ||
            (nextState == state::ImeState::Composing && currentCandidates.items && !currentCandidates.items->empty())) {
            output.candidateAction = CandidateAction::Update;
            populateCandidates(output);
        } else {
            output.candidateAction = CandidateAction::Close;
        }
    } else {
        output.compositionAction = CompositionAction::Clear;
        output.candidateAction = CandidateAction::Close;
    }

    return output;
}

EngineOutput ImeEngine::createCurrentCompositionOutput(bool isConsumed) {
    return createOutput(currentState, isConsumed, &getOrBuildConversion());
}

EngineOutput ImeEngine::clearCompositionCandidatesAndCreateOutput(bool isConsumed) {
    clearCandidates();
    return createCurrentCompositionOutput(isConsumed);
}

EngineOutput ImeEngine::refreshCompositionAfterBufferEdit() {
    invalidateCachedConversion();
    if (buffer.getLength() == 0) {
        clearCandidates();
        currentState = state::ImeState::Idle;
        return createOutput(currentState, true);
    }
    refreshPredictionsForComposition(&getOrBuildConversion());
    return createCurrentCompositionOutput(true);
}

EngineOutput ImeEngine::moveCompositionCandidateSelection(
    CandidateIndex delta,
    bool clampSelection,
    bool reopenIfHidden) {
    if (!hasVisibleCandidates()) {
        if (!reopenIfHidden) {
            return createCurrentCompositionOutput(false);
        }
        refreshPredictionsForComposition(&getOrBuildConversion());
        if (!hasVisibleCandidates()) {
            return createCurrentCompositionOutput(false);
        }
        return createCurrentCompositionOutput(true);
    }

    if (clampSelection) {
        currentCandidates.moveSelectionClamped(delta);
    } else {
        currentCandidates.moveSelection(delta);
    }
    isCompositionCandidateSelectionPrimed = true;
    return createCurrentCompositionOutput(true);
}

EngineOutput ImeEngine::selectCompositionCandidateBoundary(bool selectLast) {
    if (!hasVisibleCandidates()) {
        return createCurrentCompositionOutput(false);
    }

    if (selectLast) {
        currentCandidates.selectLast();
    } else {
        currentCandidates.selectFirst();
    }
    isCompositionCandidateSelectionPrimed = true;
    return createCurrentCompositionOutput(true);
}

EngineOutput ImeEngine::moveConversionCandidateSelection(CandidateIndex delta, bool clampSelection) {
    if (clampSelection) {
        currentCandidates.moveSelectionClamped(delta);
    } else {
        currentCandidates.moveSelection(delta);
    }
    syncSelectedSegmentCandidatePreview();
    return createOutput(currentState, true);
}

EngineOutput ImeEngine::selectConversionCandidateBoundary(bool selectLast) {
    if (selectLast) {
        currentCandidates.selectLast();
    } else {
        currentCandidates.selectFirst();
    }
    syncSelectedSegmentCandidatePreview();
    return createOutput(currentState, true);
}

void ImeEngine::clearCandidates() {
    currentCandidates.clear();
    isCompositionCandidateSelectionPrimed = false;
}

bool ImeEngine::hasVisibleCandidates() const {
    return currentCandidates.items && !currentCandidates.items->empty();
}

bool ImeEngine::tryInsertCharacter(char16_t ch) {
    buffer.reserve(buffer.getLength() + 8);
    try {
        buffer.insert(ch);
        invalidateCachedConversion();
        return true;
    } catch (const std::out_of_range&) {
        YUME_LOG_ERROR("ImeEngine", "buffer insert out_of_range, resetting session");
        resetImeSession();
        currentState = state::ImeState::Idle;
        return false;
    }
}

bool ImeEngine::tryBackspaceCompositionUnit() {
    if (buffer.getLength() == 0 || buffer.getCursor() != buffer.getLength()) {
        return false;
    }

    const auto before = getOrBuildConversion();
    const DisplayText beforeDisplay = before.confirmed + before.composing;
    if (beforeDisplay.empty()) {
        return false;
    }

    const std::u16string bufferText = buffer.getText();
    const size_t maxSuffixLength = std::min(kMaxRomajiUnitLength, bufferText.size());
    for (size_t suffixLength = 1; suffixLength <= maxSuffixLength; ++suffixLength) {
        const auto suffixStart = bufferText.size() - suffixLength;
        const auto suffix = bufferText.substr(suffixStart);
        const auto suffixConversion = converter.convertRomajiToHiragana(suffix);
        if (!suffixConversion.composing.empty() || suffixConversion.confirmed.empty()) {
            continue;
        }

        const auto prefixConversion = converter.convertRomajiToHiragana(bufferText.substr(0, suffixStart));
        const DisplayText prefixDisplay = prefixConversion.confirmed + prefixConversion.composing;
        if (beforeDisplay != prefixDisplay + suffixConversion.confirmed) {
            continue;
        }

        for (size_t i = 0; i < suffixLength; ++i) {
            buffer.backspace();
        }
        return true;
    }

    return false;
}

void ImeEngine::resetImeSession() {
    YUME_LOG_DEBUG("ImeEngine", "resetImeSession");
    buffer.clear();
    clearCandidates();
    invalidateCachedConversion();
    currentSegments.clear();
    segmentCandidates.clear();
    cachedSegmentDisplayText.clear();
    cachedSegmentDisplayRanges.clear();
    isSegmentDisplayCacheDirty = true;
    selectedSegmentIndex = 0;
}

const composition::Converter::ConversionResult& ImeEngine::getOrBuildConversion() {
    if (!cachedConversion.has_value()) {
        cachedConversion = converter.convertRomajiToHiragana(buffer.getText());
    }
    return *cachedConversion;
}

void ImeEngine::invalidateCachedConversion() {
    cachedConversion.reset();
}

std::optional<ReadingText> ImeEngine::buildReadingText(
    const composition::Converter::ConversionResult* conversion) const {
    if (buffer.getLength() == 0) {
        return std::nullopt;
    }

    const composition::Converter::ConversionResult* convResult = conversion;
    composition::Converter::ConversionResult ownedConversion;
    if (convResult == nullptr) {
        ownedConversion = converter.convertRomajiToHiragana(buffer.getText());
        convResult = &ownedConversion;
    }

    ReadingText reading;
    reading.reserve(convResult->confirmed.size() + convResult->composing.size());
    reading += convResult->confirmed;
    reading += convResult->composing;
    return reading;
}

EngineOutput ImeEngine::commitSelectedCompositionCandidate() {
    EngineOutput output;
    output.nextState = state::ImeState::Committed;
    output.inputMode = InputMode::Hiragana;
    output.isConsumed = true;
    output.commit = tryGetSelectedCandidateText().value_or(
        buildReadingText(&getOrBuildConversion()).value_or(ReadingText{}));
    output.commitAction = CommitAction::Commit;

    const auto reading = buildReadingText(&getOrBuildConversion()).value_or(ReadingText{});
    dictionary.recordCommit(reading, *output.commit);
    resetImeSession();
    currentState = state::ImeState::Committed;
    return output;
}

EngineOutput ImeEngine::commitCurrentCompositionText() {
    EngineOutput output;
    output.nextState = state::ImeState::Committed;
    output.inputMode = InputMode::Hiragana;
    output.isConsumed = true;

    const auto& conv = getOrBuildConversion();
    const auto reading = buildReadingText(&conv).value_or(ReadingText{});
    auto& commit = output.commit.emplace();
    if (isCompositionCandidateSelectionPrimed) {
        if (const auto selectedText = tryGetSelectedCandidateText(); selectedText.has_value()) {
            commit = *selectedText;
        } else {
            commit.reserve(conv.confirmed.size() + conv.composing.size());
            commit += conv.confirmed;
            commit += conv.composing;
        }
    } else {
        commit.reserve(conv.confirmed.size() + conv.composing.size());
        commit += conv.confirmed;
        commit += conv.composing;
    }
    output.commitAction = CommitAction::Commit;

    dictionary.recordCommit(reading, commit);
    resetImeSession();
    currentState = state::ImeState::Committed;
    return output;
}

EngineOutput ImeEngine::commitCompositionWithSuffix(std::u16string_view suffix) {
    EngineOutput output;
    output.nextState = state::ImeState::Committed;
    output.inputMode = InputMode::Hiragana;
    output.isConsumed = true;

    const auto& conv = getOrBuildConversion();
    const auto reading = buildReadingText(&conv).value_or(ReadingText{});
    const auto baseCommit = tryGetSelectedCandidateText().value_or(reading);

    auto& commit = output.commit.emplace();
    commit.reserve(baseCommit.size() + suffix.size());
    commit += baseCommit;
    commit += suffix;
    output.commitAction = CommitAction::Commit;

    if (!reading.empty() && !baseCommit.empty()) {
        dictionary.recordCommit(reading, baseCommit);
    }

    resetImeSession();
    currentState = state::ImeState::Committed;
    return output;
}

std::optional<SurfaceText> ImeEngine::tryGetSelectedCandidateText() const {
    return selectedCandidateText(currentCandidates);
}

size_t ImeEngine::computeDisplayCursorPosition() const {
    const auto cursor = buffer.getCursor();
    const auto prefix = buffer.getText().substr(0, cursor);
    const auto prefixConversion = converter.convertRomajiToHiragana(prefix);
    return prefixConversion.confirmed.size() + prefixConversion.composing.size();
}

std::optional<CommitText> ImeEngine::tryBuildPunctuationCommit(char16_t normalizedChar) const {
    switch (normalizedChar) {
        case u',':
            return CommitText{u'\u3001'};
        case u'.':
            return CommitText{u'\u3002'};
        case u'-':
            return CommitText{u'\u30FC'};
        case u'!':
            return CommitText{u'\uFF01'};
        case u'?':
            return CommitText{u'\uFF1F'};
        default:
            return std::nullopt;
    }
}

void ImeEngine::populateComposition(
    EngineOutput& output,
    const composition::Converter::ConversionResult* conversion) {
    if (buffer.getLength() == 0 && currentSegments.empty()) {
        output.composition.reset();
        return;
    }

    auto& composition = output.composition.emplace();

    if (sanitizeSelectedSegmentIndex()) {
        ensureSegmentDisplayCache();
        composition.text = cachedSegmentDisplayText;
        composition.segmentStart = cachedSegmentDisplayRanges[selectedSegmentIndex].start;
        composition.segmentEnd = cachedSegmentDisplayRanges[selectedSegmentIndex].end;
        composition.cursorPosition = composition.segmentEnd;
        composition.visible = true;
        return;
    }

    const composition::Converter::ConversionResult* convResult = conversion;
    composition::Converter::ConversionResult ownedConversion;
    if (convResult == nullptr) {
        ownedConversion = converter.convertRomajiToHiragana(buffer.getText());
        convResult = &ownedConversion;
    }
    composition.text.reserve(convResult->confirmed.size() + convResult->composing.size());
    composition.text += convResult->confirmed;
    composition.text += convResult->composing;
    composition.cursorPosition = computeDisplayCursorPosition();
    composition.segmentStart = 0;
    composition.segmentEnd = composition.text.length();
    composition.visible = true;
}

}
