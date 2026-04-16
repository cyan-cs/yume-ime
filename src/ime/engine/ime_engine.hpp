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



#pragma once

#include "ime/candidate/candidate_types.hpp"
#include "ime/composition/buffer.hpp"
#include "ime/composition/converter.hpp"
#include "ime/dictionary/dictionary.hpp"
#include "ime/input/key_event.hpp"
#include "ime/state/ime_states.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace yume::ime::engine {

    using Candidate = candidate::Candidate;
    using CandidateIndex = int32_t;
    using ReadingText = dictionary::ReadingText;
    using SurfaceText = dictionary::SurfaceText;
    using ReadingView = dictionary::ReadingView;
    using SurfaceView = dictionary::SurfaceView;
    using CommitText = dictionary::SurfaceText;
    using DisplayText = dictionary::SurfaceText;

    enum class InputMode {
        Latin,
        Hiragana,
    };

    enum class CompositionAction {
        None,
        Update,
        Clear,
    };

    enum class CandidateAction {
        None,
        Update,
        Close,
    };

    enum class CommitAction {
        None,
        Commit,
    };

    struct CandidateList {
        std::shared_ptr<const std::vector<Candidate>> items;
        CandidateIndex selectedIndex = 0;

        void advanceSelection() {
            if (items && !items->empty()) {
                selectedIndex = (selectedIndex + 1) % static_cast<CandidateIndex>(items->size());
            }
        }

        void clear() {
            items.reset();
            selectedIndex = 0;
        }

        bool sanitizeSelection() {
            if (!items || items->empty()) {
                selectedIndex = 0;
                return false;
            }

            const CandidateIndex size = static_cast<CandidateIndex>(items->size());
            selectedIndex = std::clamp(selectedIndex, CandidateIndex{0}, size - 1);
            return true;
        }

        void moveSelection(CandidateIndex delta) {
            if (!items || items->empty()) {
                return;
            }

            const CandidateIndex size = static_cast<CandidateIndex>(items->size());
            selectedIndex = (selectedIndex + delta + size) % size;
        }

        void moveSelectionClamped(CandidateIndex delta) {
            if (!items || items->empty()) {
                return;
            }

            const CandidateIndex size = static_cast<CandidateIndex>(items->size());
            selectedIndex = std::clamp(selectedIndex + delta, 0, size - 1);
        }

        void selectFirst() {
            if (!items || items->empty()) {
                return;
            }
            selectedIndex = 0;
        }

        void selectLast() {
            if (!items || items->empty()) {
                return;
            }
            selectedIndex = static_cast<CandidateIndex>(items->size()) - 1;
        }

        bool selectByDigit(CandidateIndex digit) {
            if (!items || items->empty()) {
                return false;
            }

            if (digit < 0 || digit >= static_cast<CandidateIndex>(items->size())) {
                return false;
            }

            selectedIndex = digit;
            return true;
        }

        const Candidate* selectedCandidate() const {
            if (!items || items->empty() || selectedIndex < 0 ||
                selectedIndex >= static_cast<CandidateIndex>(items->size())) {
                return nullptr;
            }

            return &(*items)[static_cast<size_t>(selectedIndex)];
        }

        const SurfaceText* selectedText() const {
            const Candidate* candidate = selectedCandidate();
            return candidate ? &candidate->text : nullptr;
        }
    };

    struct CompositionState {
        DisplayText text;
        size_t cursorPosition = 0;
        size_t segmentStart = 0;
        size_t segmentEnd = 0;
        bool visible = false;
    };

    struct ConversionSegment {
        ReadingText reading;
        SurfaceText text;
    };

    struct SegmentDisplayRange {
        size_t start = 0;
        size_t end = 0;
    };

    struct EngineOutput {
        state::ImeState nextState = state::ImeState::Direct;
        std::optional<CompositionState> composition;
        std::optional<CandidateList> candidates;
        std::optional<CommitText> commit;
        InputMode inputMode = InputMode::Latin;
        CompositionAction compositionAction = CompositionAction::None;
        CandidateAction candidateAction = CandidateAction::None;
        CommitAction commitAction = CommitAction::None;
        bool isConsumed = false;

        bool shouldUpdateComposition() const {
            return compositionAction == CompositionAction::Update;
        }

        bool shouldClearComposition() const {
            return compositionAction == CompositionAction::Clear;
        }

        bool shouldUpdateCandidates() const {
            return candidateAction == CandidateAction::Update;
        }

        bool shouldCloseCandidates() const {
            return candidateAction == CandidateAction::Close;
        }

        bool shouldCommitText() const {
            return commitAction == CommitAction::Commit;
        }
    };

    class ImeEngine {
    public:
        struct SessionSnapshot {
            state::ImeState currentState = state::ImeState::Direct;
            composition::Buffer buffer;
            dictionary::Dictionary::SessionSnapshot dictionary;
            CandidateList currentCandidates;
            bool isCompositionCandidateSelectionPrimed = false;
            std::optional<composition::Converter::ConversionResult> cachedConversion;
            std::vector<ConversionSegment> currentSegments;
            std::vector<CandidateList> segmentCandidates;
            DisplayText cachedSegmentDisplayText;
            std::vector<SegmentDisplayRange> cachedSegmentDisplayRanges;
            bool isSegmentDisplayCacheDirty = true;
            size_t selectedSegmentIndex = 0;
        };

        ImeEngine() = default;
        explicit ImeEngine(dictionary::DictionaryStoragePaths paths)
            : dictionary(std::move(paths)) {}

        SessionSnapshot captureSessionSnapshot() const;
        void restoreSessionSnapshot(const SessionSnapshot& snapshot);
        void discardActiveSession();
        EngineOutput processKeyEvent(const input::KeyEvent& event);
        std::optional<EngineOutput> setCandidateSelection(CandidateIndex index);
        std::optional<EngineOutput> finalizeCandidateSelection();
        std::optional<EngineOutput> abortCandidateSelection();
        void serviceIdle();
        state::ImeState getCurrentState() const { return currentState; }
        bool hasActiveSession() const;
        bool hasVisibleCandidateWindow() const;

    private:
        state::ImeState currentState = state::ImeState::Direct;
        composition::Buffer buffer;
        composition::Converter converter;
        dictionary::Dictionary dictionary;
        CandidateList currentCandidates;
        bool isCompositionCandidateSelectionPrimed = false;
        std::optional<composition::Converter::ConversionResult> cachedConversion;
        std::vector<ConversionSegment> currentSegments;
        std::vector<CandidateList> segmentCandidates;
        DisplayText cachedSegmentDisplayText;
        std::vector<SegmentDisplayRange> cachedSegmentDisplayRanges;
        bool isSegmentDisplayCacheDirty = true;
        size_t selectedSegmentIndex = 0;

        EngineOutput createOutput(
            state::ImeState nextState,
            bool isConsumed,
            const composition::Converter::ConversionResult* conversion = nullptr);
        EngineOutput handleComposingKeyEvent(
            const input::KeyEvent& event,
            bool hasCharacter,
            char16_t normalizedChar);
        EngineOutput handleConvertingKeyEvent(const input::KeyEvent& event);
        EngineOutput createCurrentCompositionOutput(bool isConsumed);
        EngineOutput clearCompositionCandidatesAndCreateOutput(bool isConsumed);
        EngineOutput refreshCompositionAfterBufferEdit();
        EngineOutput moveCompositionCandidateSelection(
            CandidateIndex delta,
            bool clampSelection,
            bool reopenIfHidden);
        EngineOutput selectCompositionCandidateBoundary(bool selectLast);
        EngineOutput moveConversionCandidateSelection(CandidateIndex delta, bool clampSelection);
        EngineOutput selectConversionCandidateBoundary(bool selectLast);
        void clearCandidates();
        bool hasVisibleCandidates() const;
        bool tryInsertCharacter(char16_t ch);
        bool tryBackspaceCompositionUnit();
        void resetImeSession();
        const composition::Converter::ConversionResult& getOrBuildConversion();
        void invalidateCachedConversion();
        std::optional<ReadingText> buildReadingText(
            const composition::Converter::ConversionResult* conversion = nullptr) const;
        CommitText buildCommittedText() const;
        EngineOutput commitCompositionWithSuffix(std::u16string_view suffix);
        void rebuildSegmentsFromReading(const ReadingText& reading);
        void persistCurrentCandidateSelection();
        void refreshPredictionsForComposition(
            const composition::Converter::ConversionResult* conversion = nullptr);
        void refreshCandidatesForSelectedSegment();
        void invalidateFollowingSegmentCandidates(size_t segmentIndex);
        void prefetchNextSegmentCandidates();
        void previewSelectedCandidate();
        void syncSelectedSegmentCandidatePreview();
        bool moveSelectedSegment(int delta);
        bool resizeSelectedSegment(int delta);
        bool advanceSegmentAfterSelection();
        EngineOutput commitSelectedCompositionCandidate();
        EngineOutput commitCurrentCompositionText();
        EngineOutput commitCurrentConversion();
        EngineOutput cancelConversionToComposing(bool reopenPredictions);
        std::pair<int, dictionary::PartOfSpeech> scoreSegmentReading(
            ReadingView reading,
            bool isTerminalSegment,
            dictionary::PartOfSpeech precedingPartOfSpeech) const;
        void invalidateSegmentDisplayCache();
        void ensureSegmentDisplayCache();
        bool sanitizeSelectedSegmentIndex();
        size_t computeDisplayCursorPosition() const;
        std::optional<CommitText> tryBuildPunctuationCommit(char16_t normalizedChar) const;
        std::optional<SurfaceText> tryGetSelectedCandidateText() const;
        void populateComposition(
            EngineOutput& output,
            const composition::Converter::ConversionResult* conversion = nullptr);
        void populateCandidates(EngineOutput& output);
    };

}
