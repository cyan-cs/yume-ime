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

#include <limits>

namespace yume::ime::engine {

namespace {

bool isParticleChar(char16_t ch) {
    return ch == u'\u306F' || ch == u'\u3092' || ch == u'\u304C' || ch == u'\u306B' ||
           ch == u'\u3067' || ch == u'\u3068' || ch == u'\u3078' || ch == u'\u3082' ||
           ch == u'\u306E';
}

bool isSmallKana(char16_t ch) {
    return ch == u'\u3083' || ch == u'\u3085' || ch == u'\u3087' ||
           ch == u'\u3041' || ch == u'\u3043' || ch == u'\u3045' ||
           ch == u'\u3047' || ch == u'\u3049' || ch == u'\u3063';
}

bool isAsciiDigit(char16_t ch) {
    return ch >= u'0' && ch <= u'9';
}

bool isAsciiLetter(char16_t ch) {
    return (ch >= u'a' && ch <= u'z') || (ch >= u'A' && ch <= u'Z');
}

bool isHiragana(char16_t ch) {
    return ch >= u'\u3041' && ch <= u'\u3096';
}

bool isKatakana(char16_t ch) {
    return ch >= u'\u30A1' && ch <= u'\u30FA';
}

bool isNumericReading(ReadingView reading) {
    return !reading.empty() && std::all_of(reading.begin(), reading.end(), [](char16_t ch) {
        return isAsciiDigit(ch);
    });
}

bool isAsciiWord(std::u16string_view text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char16_t ch) {
        return isAsciiLetter(ch);
    });
}

bool isKatakanaWord(std::u16string_view text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](char16_t ch) {
        return isKatakana(ch) || ch == u'\u30FC';
    });
}

bool isKanaReading(ReadingView reading) {
    return !reading.empty() && std::all_of(reading.begin(), reading.end(), [](char16_t ch) {
        return isHiragana(ch) || ch == u'\u30FC';
    });
}

bool shouldDemoteFallbackPrediction(std::u16string_view reading, const Candidate& candidate) {
    if (candidate.text == reading) {
        return true;
    }

    if (!isKanaReading(reading)) {
        return false;
    }

    return isAsciiWord(candidate.text) || isKatakanaWord(candidate.text);
}

std::shared_ptr<const std::vector<Candidate>> reorderFallbackPredictions(
    ReadingView reading,
    dictionary::PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface,
    std::shared_ptr<const std::vector<Candidate>> items) {
    if (!items || items->size() < 2 || precedingSurface.empty() || isNumericReading(reading)) {
        return items;
    }

    if (precedingPartOfSpeech != dictionary::PartOfSpeech::Particle &&
        precedingPartOfSpeech != dictionary::PartOfSpeech::Copula &&
        precedingPartOfSpeech != dictionary::PartOfSpeech::Ending &&
        precedingPartOfSpeech != dictionary::PartOfSpeech::Modal) {
        return items;
    }

    auto reordered = std::make_shared<std::vector<Candidate>>();
    reordered->reserve(items->size());

    for (const auto& candidate : *items) {
        if (!shouldDemoteFallbackPrediction(reading, candidate)) {
            reordered->push_back(candidate);
        }
    }
    for (const auto& candidate : *items) {
        if (shouldDemoteFallbackPrediction(reading, candidate)) {
            reordered->push_back(candidate);
        }
    }

    return reordered;
}

size_t predictionLimitForReading(ReadingView reading) {
    if (reading.empty()) {
        return 0;
    }

    if (isNumericReading(reading)) {
        return 8;
    }

    if (reading.size() <= 1) {
        return 4;
    }
    if (reading.size() <= 2) {
        return 5;
    }
    if (reading.size() <= 4) {
        return 6;
    }
    return 8;
}

constexpr size_t kMaxSegmentLength = 8;
constexpr size_t kPartOfSpeechStateCount = static_cast<size_t>(dictionary::PartOfSpeech::Modal) + 1;

size_t partOfSpeechIndex(dictionary::PartOfSpeech partOfSpeech) {
    return static_cast<size_t>(partOfSpeech);
}

dictionary::PartOfSpeech partOfSpeechFromIndex(size_t index) {
    if (index >= kPartOfSpeechStateCount) {
        return dictionary::PartOfSpeech::Unknown;
    }
    return static_cast<dictionary::PartOfSpeech>(index);
}

dictionary::PartOfSpeech precedingPartOfSpeechForSegment(
    const dictionary::Dictionary& dictionary,
    const std::vector<ConversionSegment>& currentSegments,
    size_t selectedSegmentIndex) {
    if (selectedSegmentIndex == 0 || selectedSegmentIndex > currentSegments.size()) {
        return dictionary.lastCommittedPartOfSpeech();
    }

    const auto& previous = currentSegments[selectedSegmentIndex - 1];
    const auto previousPartOfSpeech = dictionary.lookupPartOfSpeech(previous.reading, previous.text);
    if (previousPartOfSpeech != dictionary::PartOfSpeech::Unknown) {
        return previousPartOfSpeech;
    }
    return dictionary.lastCommittedPartOfSpeech();
}

dictionary::SurfaceText precedingSurfaceForSegment(
    const dictionary::Dictionary& dictionary,
    const std::vector<ConversionSegment>& currentSegments,
    size_t selectedSegmentIndex) {
    if (selectedSegmentIndex == 0 || selectedSegmentIndex > currentSegments.size()) {
        return dictionary.lastCommittedSurface();
    }

    return currentSegments[selectedSegmentIndex - 1].text;
}

}
CommitText ImeEngine::buildCommittedText() const {
    CommitText text;
    size_t totalLength = 0;
    for (const auto& segment : currentSegments) {
        totalLength += segment.text.size();
    }
    text.reserve(totalLength);
    for (const auto& segment : currentSegments) {
        text += segment.text;
    }
    return text;
}

void ImeEngine::rebuildSegmentsFromReading(const ReadingText& reading) {
    currentSegments.clear();
    segmentCandidates.clear();
    selectedSegmentIndex = 0;
    invalidateSegmentDisplayCache();

    if (reading.empty()) {
        return;
    }

    const size_t length = reading.size();
    const int kUnreachable = std::numeric_limits<int>::min() / 4;

    struct PathState {
        int score = 0;
        size_t previousCursor = 0;
        size_t previousPartOfSpeechIndex = 0;
        bool reachable = false;
    };

    std::vector<std::vector<PathState>> bestStates(
        length + 1,
        std::vector<PathState>(kPartOfSpeechStateCount, PathState{kUnreachable, 0, 0, false}));
    const size_t initialPartOfSpeechIndex = partOfSpeechIndex(dictionary.lastCommittedPartOfSpeech());
    bestStates[0][initialPartOfSpeechIndex] = {0, 0, initialPartOfSpeechIndex, true};

    for (size_t start = 0; start < length; ++start) {
        if (isSmallKana(reading[start])) {
            continue;
        }

        const size_t limit = std::min(length, start + kMaxSegmentLength);
        for (size_t previousPartOfSpeechIndex = 0;
             previousPartOfSpeechIndex < kPartOfSpeechStateCount;
             ++previousPartOfSpeechIndex) {
            const auto& state = bestStates[start][previousPartOfSpeechIndex];
            if (!state.reachable) {
                continue;
            }

            for (size_t end = start + 1; end <= limit; ++end) {
                if (end < length && isSmallKana(reading[end])) {
                    continue;
                }

                const ReadingView segmentView(reading.data() + start, end - start);
                const auto [score, resultingPartOfSpeech] = scoreSegmentReading(
                    segmentView,
                    end == length,
                    partOfSpeechFromIndex(previousPartOfSpeechIndex));
                if (score == kUnreachable) {
                    continue;
                }

                const int combined = state.score + score;
                const size_t nextPartOfSpeechIndex = partOfSpeechIndex(resultingPartOfSpeech);
                auto& nextState = bestStates[end][nextPartOfSpeechIndex];
                if (!nextState.reachable || combined > nextState.score) {
                    nextState.score = combined;
                    nextState.previousCursor = start;
                    nextState.previousPartOfSpeechIndex = previousPartOfSpeechIndex;
                    nextState.reachable = true;
                }
            }
        }
    }

    size_t bestTerminalPartOfSpeechIndex = initialPartOfSpeechIndex;
    int bestTerminalScore = kUnreachable;
    for (size_t index = 0; index < kPartOfSpeechStateCount; ++index) {
        const auto& state = bestStates[length][index];
        if (state.reachable && state.score > bestTerminalScore) {
            bestTerminalScore = state.score;
            bestTerminalPartOfSpeechIndex = index;
        }
    }

    std::vector<size_t> nextBreak(length + 1, length);
    size_t cursor = length;
    size_t currentPartOfSpeechIndex = bestTerminalPartOfSpeechIndex;
    while (cursor > 0) {
        const auto& state = bestStates[cursor][currentPartOfSpeechIndex];
        if (!state.reachable || state.previousCursor >= cursor) {
            break;
        }
        nextBreak[state.previousCursor] = cursor;
        currentPartOfSpeechIndex = state.previousPartOfSpeechIndex;
        cursor = state.previousCursor;
    }

    cursor = 0;
    while (cursor < length) {
        size_t end = nextBreak[cursor];
        if (end <= cursor || end > length) {
            end = std::min(length, cursor + 1);
            while (end < length && isSmallKana(reading[end])) {
                ++end;
            }
        }

        ReadingText segmentReading(reading.substr(cursor, end - cursor));
        currentSegments.push_back({segmentReading, segmentReading});
        cursor = end;
    }

    segmentCandidates.resize(currentSegments.size());
}

void ImeEngine::persistCurrentCandidateSelection() {
    if (!sanitizeSelectedSegmentIndex() || selectedSegmentIndex >= segmentCandidates.size()) {
        return;
    }

    segmentCandidates[selectedSegmentIndex] = currentCandidates;
}

void ImeEngine::refreshPredictionsForComposition(
    const composition::Converter::ConversionResult* conversion) {
    if (buffer.getLength() == 0) {
        clearCandidates();
        return;
    }

    const auto reading = buildReadingText(conversion);
    if (!reading.has_value() || reading->empty()) {
        clearCandidates();
        return;
    }

    currentCandidates.items = dictionary.getPredictionsShared(
        *reading,
        predictionLimitForReading(*reading),
        dictionary.lastCommittedPartOfSpeech(),
        dictionary.lastCommittedSurface());
    currentCandidates.items = reorderFallbackPredictions(
        *reading,
        dictionary.lastCommittedPartOfSpeech(),
        dictionary.lastCommittedSurface(),
        currentCandidates.items);
    currentCandidates.selectedIndex = 0;
    isCompositionCandidateSelectionPrimed = false;
    if (!currentCandidates.items || currentCandidates.items->empty()) {
        clearCandidates();
    }
}

void ImeEngine::refreshCandidatesForSelectedSegment() {
    if (!sanitizeSelectedSegmentIndex()) {
        clearCandidates();
        return;
    }

    if (selectedSegmentIndex < segmentCandidates.size()) {
        auto& cachedCandidates = segmentCandidates[selectedSegmentIndex];
        if (cachedCandidates.items && !cachedCandidates.items->empty()) {
            cachedCandidates.sanitizeSelection();
            currentCandidates = cachedCandidates;
            previewSelectedCandidate();
            return;
        }
    }

    currentCandidates.items = dictionary.getCandidatesShared(
        currentSegments[selectedSegmentIndex].reading,
        precedingPartOfSpeechForSegment(dictionary, currentSegments, selectedSegmentIndex),
        precedingSurfaceForSegment(dictionary, currentSegments, selectedSegmentIndex));
    currentCandidates.selectedIndex = 0;
    persistCurrentCandidateSelection();
    previewSelectedCandidate();
}

void ImeEngine::invalidateFollowingSegmentCandidates(size_t segmentIndex) {
    if (segmentIndex + 1 >= segmentCandidates.size()) {
        return;
    }

    for (size_t index = segmentIndex + 1; index < segmentCandidates.size(); ++index) {
        segmentCandidates[index].clear();
    }
}

void ImeEngine::prefetchNextSegmentCandidates() {
    if (!sanitizeSelectedSegmentIndex()) {
        return;
    }

    const size_t nextSegmentIndex = selectedSegmentIndex + 1;
    if (nextSegmentIndex >= currentSegments.size() || nextSegmentIndex >= segmentCandidates.size()) {
        return;
    }

    auto& cachedCandidates = segmentCandidates[nextSegmentIndex];
    if (cachedCandidates.items && !cachedCandidates.items->empty()) {
        cachedCandidates.sanitizeSelection();
        return;
    }

    cachedCandidates.items = dictionary.getCandidatesShared(
        currentSegments[nextSegmentIndex].reading,
        precedingPartOfSpeechForSegment(dictionary, currentSegments, nextSegmentIndex),
        precedingSurfaceForSegment(dictionary, currentSegments, nextSegmentIndex));
    cachedCandidates.selectedIndex = 0;
    cachedCandidates.sanitizeSelection();
}

void ImeEngine::previewSelectedCandidate() {
    if (!sanitizeSelectedSegmentIndex()) {
        return;
    }

    const SurfaceText previousText = currentSegments[selectedSegmentIndex].text;
    if (currentCandidates.sanitizeSelection()) {
        currentSegments[selectedSegmentIndex].text = *currentCandidates.selectedText();
    } else {
        currentSegments[selectedSegmentIndex].text = currentSegments[selectedSegmentIndex].reading;
    }
    if (currentSegments[selectedSegmentIndex].text != previousText) {
        invalidateFollowingSegmentCandidates(selectedSegmentIndex);
    }
    invalidateSegmentDisplayCache();
    prefetchNextSegmentCandidates();
}

void ImeEngine::syncSelectedSegmentCandidatePreview() {
    persistCurrentCandidateSelection();
    previewSelectedCandidate();
}

bool ImeEngine::moveSelectedSegment(int delta) {
    if (!sanitizeSelectedSegmentIndex()) {
        return false;
    }

    const int next = static_cast<int>(selectedSegmentIndex) + delta;
    if (next < 0 || next >= static_cast<int>(currentSegments.size())) {
        return false;
    }

    persistCurrentCandidateSelection();
    previewSelectedCandidate();
    selectedSegmentIndex = static_cast<size_t>(next);
    refreshCandidatesForSelectedSegment();
    return true;
}

bool ImeEngine::resizeSelectedSegment(int delta) {
    if (!sanitizeSelectedSegmentIndex()) {
        return false;
    }

    if (delta > 0) {
        if (selectedSegmentIndex + 1 >= currentSegments.size()) {
            return false;
        }

        auto& current = currentSegments[selectedSegmentIndex];
        auto& next = currentSegments[selectedSegmentIndex + 1];
        current.reading += next.reading;
        current.text = current.reading;
        currentSegments.erase(currentSegments.begin() + static_cast<std::ptrdiff_t>(selectedSegmentIndex + 1));
        if (!segmentCandidates.empty()) {
            segmentCandidates[selectedSegmentIndex].clear();
            segmentCandidates.erase(
                segmentCandidates.begin() + static_cast<std::ptrdiff_t>(selectedSegmentIndex + 1));
        }
    } else if (delta < 0) {
        if (selectedSegmentIndex == 0) {
            return false;
        }

        auto& previous = currentSegments[selectedSegmentIndex - 1];
        auto& current = currentSegments[selectedSegmentIndex];
        previous.reading += current.reading;
        previous.text = previous.reading;
        currentSegments.erase(currentSegments.begin() + static_cast<std::ptrdiff_t>(selectedSegmentIndex));
        if (!segmentCandidates.empty()) {
            segmentCandidates[selectedSegmentIndex - 1].clear();
            segmentCandidates.erase(segmentCandidates.begin() + static_cast<std::ptrdiff_t>(selectedSegmentIndex));
        }
        --selectedSegmentIndex;
    } else {
        return false;
    }

    clearCandidates();
    invalidateSegmentDisplayCache();
    refreshCandidatesForSelectedSegment();
    return true;
}

bool ImeEngine::advanceSegmentAfterSelection() {
    if (!sanitizeSelectedSegmentIndex()) {
        return false;
    }

    persistCurrentCandidateSelection();
    if (selectedSegmentIndex + 1 >= currentSegments.size()) {
        return false;
    }

    ++selectedSegmentIndex;
    refreshCandidatesForSelectedSegment();
    return true;
}

EngineOutput ImeEngine::commitCurrentConversion() {
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

EngineOutput ImeEngine::cancelConversionToComposing(bool reopenPredictions) {
    currentState = state::ImeState::Composing;
    clearCandidates();
    currentSegments.clear();
    selectedSegmentIndex = 0;
    if (reopenPredictions) {
        refreshPredictionsForComposition(&getOrBuildConversion());
    }
    return createOutput(currentState, true, &getOrBuildConversion());
}

std::pair<int, dictionary::PartOfSpeech> ImeEngine::scoreSegmentReading(
    ReadingView reading,
    bool isTerminalSegment,
    dictionary::PartOfSpeech precedingPartOfSpeech) const {
    const int kUnreachable = std::numeric_limits<int>::min() / 4;
    if (reading.empty()) {
        return {kUnreachable, dictionary::PartOfSpeech::Unknown};
    }

    if (isSmallKana(reading.front())) {
        return {kUnreachable, dictionary::PartOfSpeech::Unknown};
    }

    int score = -20;
    dictionary::PartOfSpeech resultingPartOfSpeech = dictionary::PartOfSpeech::Unknown;
    const auto candidates = dictionary.getCandidatesShared(reading, precedingPartOfSpeech);
    if (candidates && !candidates->empty()) {
        resultingPartOfSpeech = dictionary.lookupPartOfSpeech(reading, candidates->front().text);
        score += 220 + static_cast<int>(reading.size()) * 12;
        score += std::clamp(candidates->front().score / 4, -60, 120);
        // 長い語句の途中で 1 文字ごとに切り過ぎない。
        if (!isTerminalSegment && reading.size() == 1 && !isParticleChar(reading.front())) {
            score -= 120;
        }
    } else if (reading.size() == 1 && isParticleChar(reading.front())) {
        resultingPartOfSpeech = dictionary::PartOfSpeech::Particle;
        score += 140;
    } else {
        score -= static_cast<int>(reading.size()) * 6;
        if (reading.size() == 1) {
            score -= 15;
        }
        if (!isTerminalSegment && reading.size() > 4) {
            score -= 25;
        }
    }

    if (!isTerminalSegment && isParticleChar(reading.back())) {
        score += 80;
    }

    if (reading.size() >= 2 && isParticleChar(reading.front())) {
        score -= 60;
    }

    return {score, resultingPartOfSpeech};
}

void ImeEngine::invalidateSegmentDisplayCache() {
    isSegmentDisplayCacheDirty = true;
}

void ImeEngine::ensureSegmentDisplayCache() {
    if (!isSegmentDisplayCacheDirty) {
        return;
    }

    cachedSegmentDisplayText.clear();
    cachedSegmentDisplayRanges.clear();
    cachedSegmentDisplayRanges.resize(currentSegments.size());

    size_t totalLength = 0;
    for (const auto& segment : currentSegments) {
        totalLength += segment.text.size();
    }
    cachedSegmentDisplayText.reserve(totalLength);

    size_t offset = 0;
    for (size_t i = 0; i < currentSegments.size(); ++i) {
        cachedSegmentDisplayRanges[i].start = offset;
        cachedSegmentDisplayText += currentSegments[i].text;
        offset += currentSegments[i].text.size();
        cachedSegmentDisplayRanges[i].end = offset;
    }

    isSegmentDisplayCacheDirty = false;
}

bool ImeEngine::sanitizeSelectedSegmentIndex() {
    if (currentSegments.empty()) {
        selectedSegmentIndex = 0;
        return false;
    }

    if (selectedSegmentIndex >= currentSegments.size()) {
        selectedSegmentIndex = currentSegments.size() - 1;
    }
    return true;
}

void ImeEngine::populateCandidates(EngineOutput& output) {
    if (!currentCandidates.sanitizeSelection()) {
        output.candidates.reset();
        return;
    }

    auto& candidates = output.candidates.emplace();
    candidates.items = currentCandidates.items;
    candidates.selectedIndex = currentCandidates.selectedIndex;
}

}
