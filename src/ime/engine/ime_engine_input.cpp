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

#include "ime/composition/normalizer.hpp"

namespace yume::ime::engine {

namespace {

std::optional<CandidateIndex> digitSelectionFromKey(input::KeyCode keyCode) {
    switch (keyCode) {
        case input::KeyCode::Num1: return 0;
        case input::KeyCode::Num2: return 1;
        case input::KeyCode::Num3: return 2;
        case input::KeyCode::Num4: return 3;
        case input::KeyCode::Num5: return 4;
        case input::KeyCode::Num6: return 5;
        case input::KeyCode::Num7: return 6;
        case input::KeyCode::Num8: return 7;
        case input::KeyCode::Num9: return 8;
        case input::KeyCode::Num0: return 9;
        default: return std::nullopt;
    }
}

constexpr CandidateIndex kCandidatePageSize = 6;

bool isAsciiDigit(char16_t ch) {
    return ch >= u'0' && ch <= u'9';
}

bool isAsciiLetter(char16_t ch) {
    return (ch >= u'a' && ch <= u'z') || (ch >= u'A' && ch <= u'Z');
}

bool shouldCommitShiftedAsciiDirectly(const input::KeyEvent& event, char16_t ch) {
    return event.shiftPressed && isAsciiLetter(ch);
}

bool isCompositionInputChar(char16_t ch) {
    return isAsciiLetter(ch) || isAsciiDigit(ch) || ch == u'\'';
}

}
EngineOutput ImeEngine::processKeyEvent(const input::KeyEvent& event) {
    if (event.isKeyDown &&
        (event.keyCode == input::KeyCode::ToggleIme ||
         (event.keyCode == input::KeyCode::Space && event.ctrlPressed))) {
        currentState = (currentState == state::ImeState::Direct) ? state::ImeState::Idle : state::ImeState::Direct;
        if (currentState == state::ImeState::Direct) {
            resetImeSession();
        }
        return createOutput(currentState, true);
    }

    if (event.isKeyDown && event.keyCode == input::KeyCode::ModeLatin) {
        currentState = state::ImeState::Direct;
        resetImeSession();
        return createOutput(currentState, true);
    }

    if (event.isKeyDown && event.keyCode == input::KeyCode::ModeHiragana) {
        if (currentState == state::ImeState::Direct) {
            currentState = state::ImeState::Idle;
        }
        return createOutput(currentState, true);
    }

    if (currentState == state::ImeState::Committed && event.isKeyDown) {
        currentState = state::ImeState::Idle;
    }

    if (!event.isKeyDown || currentState == state::ImeState::Direct) {
        return createOutput(currentState, false);
    }

    const bool hasCharacter = event.character.has_value();
    const char16_t normalizedChar = hasCharacter
        ? composition::Normalizer::toHalfWidthChar(*event.character)
        : u'\0';

    switch (currentState) {
        case state::ImeState::Idle:
            if (hasCharacter) {
                if (shouldCommitShiftedAsciiDirectly(event, normalizedChar)) {
                    EngineOutput output;
                    output.nextState = state::ImeState::Committed;
                    output.inputMode = InputMode::Hiragana;
                    output.commit = CommitText(1, normalizedChar);
                    output.commitAction = CommitAction::Commit;
                    output.isConsumed = true;
                    currentState = state::ImeState::Committed;
                    return output;
                }

                if (const auto punctuation = tryBuildPunctuationCommit(normalizedChar); punctuation.has_value()) {
                    EngineOutput output;
                    output.nextState = state::ImeState::Committed;
                    output.inputMode = InputMode::Hiragana;
                    output.commit = punctuation;
                    output.commitAction = CommitAction::Commit;
                    output.isConsumed = true;
                    currentState = state::ImeState::Committed;
                    return output;
                }

                if (!isCompositionInputChar(normalizedChar)) {
                    EngineOutput output;
                    output.nextState = state::ImeState::Committed;
                    output.inputMode = InputMode::Hiragana;
                    output.commit = CommitText(1, normalizedChar);
                    output.commitAction = CommitAction::Commit;
                    output.isConsumed = true;
                    currentState = state::ImeState::Committed;
                    return output;
                }

                if (!tryInsertCharacter(normalizedChar)) {
                    return createOutput(currentState, false);
                }
                currentState = state::ImeState::Composing;
                refreshPredictionsForComposition(&getOrBuildConversion());
                return createOutput(currentState, true, &getOrBuildConversion());
            }
            break;

        case state::ImeState::Composing:
            return handleComposingKeyEvent(event, hasCharacter, normalizedChar);

        case state::ImeState::Converting:
            return handleConvertingKeyEvent(event);

        case state::ImeState::Committed:
        default:
            break;
    }

    return createOutput(currentState, false);
}

EngineOutput ImeEngine::handleComposingKeyEvent(
    const input::KeyEvent& event,
    bool hasCharacter,
    char16_t normalizedChar) {
    if (event.keyCode == input::KeyCode::Escape) {
        if (hasVisibleCandidates()) {
            return clearCompositionCandidatesAndCreateOutput(true);
        }
        return createCurrentCompositionOutput(false);
    }

    if (event.keyCode == input::KeyCode::Backspace) {
        if (!tryBackspaceCompositionUnit()) {
            buffer.backspace();
        }
        return refreshCompositionAfterBufferEdit();
    }

    if (event.keyCode == input::KeyCode::Delete) {
        buffer.deleteForward();
        return refreshCompositionAfterBufferEdit();
    }

    if (event.keyCode == input::KeyCode::Left) {
        buffer.moveCursorLeft();
        return clearCompositionCandidatesAndCreateOutput(true);
    }

    if (event.keyCode == input::KeyCode::Right) {
        buffer.moveCursorRight();
        return clearCompositionCandidatesAndCreateOutput(true);
    }

    if (event.keyCode == input::KeyCode::Down) {
        return moveCompositionCandidateSelection(1, false, true);
    }

    if (event.keyCode == input::KeyCode::Up) {
        return moveCompositionCandidateSelection(-1, false, false);
    }

    if (event.keyCode == input::KeyCode::Tab && !event.shiftPressed) {
        return moveCompositionCandidateSelection(1, false, true);
    }

    if (event.keyCode == input::KeyCode::Tab && event.shiftPressed) {
        return moveCompositionCandidateSelection(-1, false, false);
    }

    if (event.keyCode == input::KeyCode::PageDown) {
        return moveCompositionCandidateSelection(kCandidatePageSize, true, true);
    }

    if (event.keyCode == input::KeyCode::PageUp) {
        return moveCompositionCandidateSelection(-kCandidatePageSize, true, false);
    }

    if (event.keyCode == input::KeyCode::Home) {
        return selectCompositionCandidateBoundary(false);
    }

    if (event.keyCode == input::KeyCode::End) {
        return selectCompositionCandidateBoundary(true);
    }

    if (!(hasCharacter && isAsciiDigit(normalizedChar))) {
        if (const auto digit = digitSelectionFromKey(event.keyCode); digit.has_value()) {
            if (currentCandidates.selectByDigit(*digit) && hasVisibleCandidates()) {
                return commitSelectedCompositionCandidate();
            }
        }
    }

    if (event.keyCode == input::KeyCode::Enter) {
        return commitCurrentCompositionText();
    }

    if (event.keyCode == input::KeyCode::Space) {
        if (!event.shiftPressed) {
            if (hasVisibleCandidates()) {
                if (!isCompositionCandidateSelectionPrimed) {
                    isCompositionCandidateSelectionPrimed = true;
                    return createCurrentCompositionOutput(true);
                }
                if (currentCandidates.items && currentCandidates.items->size() > 1) {
                    return moveCompositionCandidateSelection(1, false, true);
                }
                return createCurrentCompositionOutput(true);
            }

            refreshPredictionsForComposition(&getOrBuildConversion());
            if (hasVisibleCandidates()) {
                isCompositionCandidateSelectionPrimed = true;
                return createCurrentCompositionOutput(true);
            }
            return clearCompositionCandidatesAndCreateOutput(true);
        }
        const auto& conv = getOrBuildConversion();
        const auto reading = buildReadingText(&conv).value_or(ReadingText{});
        rebuildSegmentsFromReading(reading);
        refreshCandidatesForSelectedSegment();
        currentState = state::ImeState::Converting;
        return createOutput(currentState, true, &conv);
    }

    if (hasCharacter) {
        if (normalizedChar == u'-') {
            if (!tryInsertCharacter(u'\u30FC')) {
                return createOutput(currentState, false);
            }
            refreshPredictionsForComposition(&getOrBuildConversion());
            return createCurrentCompositionOutput(true);
        }

        if (shouldCommitShiftedAsciiDirectly(event, normalizedChar)) {
            return commitCompositionWithSuffix(std::u16string_view(&normalizedChar, 1));
        }

        if (const auto punctuation = tryBuildPunctuationCommit(normalizedChar); punctuation.has_value()) {
            EngineOutput output;
            output.nextState = state::ImeState::Committed;
            output.inputMode = InputMode::Hiragana;
            output.isConsumed = true;
            const auto reading = buildReadingText(&getOrBuildConversion()).value_or(ReadingText{});
            const auto baseCommit = tryGetSelectedCandidateText().value_or(reading);
            auto& commit = output.commit.emplace();
            commit.reserve(baseCommit.size() + punctuation->size());
            commit += baseCommit;
            commit += *punctuation;
            output.commitAction = CommitAction::Commit;
            dictionary.recordCommit(reading, baseCommit);
            resetImeSession();
            currentState = state::ImeState::Committed;
            return output;
        }

        if (!isCompositionInputChar(normalizedChar)) {
            return commitCompositionWithSuffix(std::u16string_view(&normalizedChar, 1));
        }

        if (!tryInsertCharacter(normalizedChar)) {
            return createOutput(currentState, false);
        }
        refreshPredictionsForComposition(&getOrBuildConversion());
        return createCurrentCompositionOutput(true);
    }

    return createCurrentCompositionOutput(false);
}

EngineOutput ImeEngine::handleConvertingKeyEvent(const input::KeyEvent& event) {
    if (event.keyCode == input::KeyCode::Enter) {
        syncSelectedSegmentCandidatePreview();
        if (advanceSegmentAfterSelection()) {
            return createOutput(currentState, true);
        }
        return commitCurrentConversion();
    }

    if (const auto digit = digitSelectionFromKey(event.keyCode); digit.has_value()) {
        if (currentCandidates.selectByDigit(*digit)) {
            syncSelectedSegmentCandidatePreview();
            if (advanceSegmentAfterSelection()) {
                return createOutput(currentState, true);
            }
            return commitCurrentConversion();
        }
        return createOutput(currentState, false);
    }

    if (event.keyCode == input::KeyCode::Escape || event.keyCode == input::KeyCode::Backspace) {
        return cancelConversionToComposing(false);
    }

    if (event.keyCode == input::KeyCode::Space || event.keyCode == input::KeyCode::Down) {
        return moveConversionCandidateSelection(1, false);
    }

    if (event.keyCode == input::KeyCode::Up) {
        return moveConversionCandidateSelection(-1, false);
    }

    if (event.keyCode == input::KeyCode::PageDown) {
        return moveConversionCandidateSelection(kCandidatePageSize, true);
    }

    if (event.keyCode == input::KeyCode::PageUp) {
        return moveConversionCandidateSelection(-kCandidatePageSize, true);
    }

    if (event.keyCode == input::KeyCode::Home) {
        return selectConversionCandidateBoundary(false);
    }

    if (event.keyCode == input::KeyCode::End) {
        return selectConversionCandidateBoundary(true);
    }

    if (event.keyCode == input::KeyCode::Left) {
        if (event.shiftPressed) {
            if (resizeSelectedSegment(-1)) {
                return createOutput(currentState, true);
            }
            return createOutput(currentState, false);
        }
        if (moveSelectedSegment(-1)) {
            return createOutput(currentState, true);
        }
        return createOutput(currentState, false);
    }

    if (event.keyCode == input::KeyCode::Right) {
        if (event.shiftPressed) {
            if (resizeSelectedSegment(1)) {
                return createOutput(currentState, true);
            }
            return createOutput(currentState, false);
        }
        if (moveSelectedSegment(1)) {
            return createOutput(currentState, true);
        }
        return createOutput(currentState, false);
    }

    return createOutput(currentState, false);
}

}
