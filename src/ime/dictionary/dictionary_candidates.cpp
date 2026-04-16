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

#include "ime/dictionary/dictionary_internal.hpp"

#include <vector>

namespace yume::ime::dictionary {

void Dictionary::getCandidates(
    const std::u16string& reading,
    CandidateVector& outCandidates,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface) const {
    const auto items = getCandidatesShared(reading, precedingPartOfSpeech, precedingSurface);
    outCandidates = items ? *items : CandidateVector{};
}

std::shared_ptr<const Dictionary::CandidateVector> Dictionary::getCandidatesShared(
    ReadingView reading,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface) const {
    std::scoped_lock lock(mutex);
    const ExactCandidateCacheKey key{
        std::u16string(reading),
        precedingPartOfSpeech,
        std::u16string(precedingSurface),
    };
    const CacheGeneration generation = combinedGeneration();
    auto& cacheEntry = exactCandidateCache[key];
    if (cacheEntry.items && cacheEntry.generation == generation) {
        return cacheEntry.items;
    }

    cacheEntry.generation = generation;
    cacheEntry.items = buildExactCandidates(reading, precedingPartOfSpeech, precedingSurface);
    return cacheEntry.items;
}

void Dictionary::getPredictions(
    ReadingView readingPrefix,
    CandidateVector& outCandidates,
    size_t limit,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface) const {
    const auto items = getPredictionsShared(readingPrefix, limit, precedingPartOfSpeech, precedingSurface);
    outCandidates = items ? *items : CandidateVector{};
}

std::shared_ptr<const Dictionary::CandidateVector> Dictionary::getPredictionsShared(
    ReadingView readingPrefix,
    size_t limit,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface) const {
    std::scoped_lock lock(mutex);
    if (readingPrefix.empty()) {
        static const auto empty = std::make_shared<const CandidateVector>();
        return empty;
    }

    auto& cacheEntry = predictionCache[PredictionCacheKey{
        std::u16string(readingPrefix),
        limit,
        precedingPartOfSpeech,
        std::u16string(precedingSurface),
    }];
    const CacheGeneration generation = combinedGeneration();
    if (cacheEntry.items && cacheEntry.generation == generation) {
        return cacheEntry.items;
    }

    cacheEntry.generation = generation;
    cacheEntry.items = buildPredictionCandidates(readingPrefix, limit, precedingPartOfSpeech, precedingSurface);
    return cacheEntry.items;
}

bool Dictionary::hasExactReading(ReadingView reading) const {
    std::scoped_lock lock(mutex);
    if (const auto* defaultExact = defaultDb().findExact(reading)) {
        for (const EntryIndex index : *defaultExact) {
            const auto& entry = defaultDb().entryAt(index);
            if (!blackDb.isBlocked(entry.reading, entry.text)) {
                return true;
            }
        }
    }

    if (const auto* userExact = userDb.findExact(reading)) {
        for (const EntryIndex index : *userExact) {
            const auto& entry = userDb.entryAt(index);
            if (!blackDb.isBlocked(entry.reading, entry.text)) {
                return true;
            }
        }
    }

    return false;
}

std::shared_ptr<const Dictionary::CandidateVector> Dictionary::buildExactCandidates(
    ReadingView reading,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface) const {
    auto candidates = std::make_shared<CandidateVector>();
    candidates->reserve(8);
    std::unordered_map<detail::CandidateKey, size_t, detail::CandidateKeyHash> candidateIndices;
    candidateIndices.reserve(8);

    if (const auto* defaultExact = defaultDb().findExact(reading)) {
        for (const EntryIndex index : *defaultExact) {
            const auto& entry = defaultDb().entryAt(index);
            detail::appendEntryCandidate(
                *candidates,
                candidateIndices,
                entry,
                blackDb,
                detail::contextualPartOfSpeechBoost(entry, precedingPartOfSpeech) +
                    detail::surfaceGrammarBoost(entry, precedingPartOfSpeech, precedingSurface) +
                    detail::lexicalCategoryBoost(entry, precedingPartOfSpeech) +
                    userDb.contextScore(precedingSurface, entry.reading, entry.text));
        }
    }

    if (const auto* userExact = userDb.findExact(reading)) {
        for (const EntryIndex index : *userExact) {
            const auto& entry = userDb.entryAt(index);
            detail::appendEntryCandidate(
                *candidates,
                candidateIndices,
                entry,
                blackDb,
                detail::contextualPartOfSpeechBoost(entry, precedingPartOfSpeech) +
                    detail::surfaceGrammarBoost(entry, precedingPartOfSpeech, precedingSurface) +
                    detail::lexicalCategoryBoost(entry, precedingPartOfSpeech) +
                    userDb.contextScore(precedingSurface, entry.reading, entry.text));
        }
    }

    if (!reading.empty() && !blackDb.isBlocked(reading, reading)) {
        const std::u16string raw(reading);
        detail::appendUniqueCandidate(
            *candidates,
            candidateIndices,
            {raw, raw, detail::rawReadingGrammarBoost(reading, precedingPartOfSpeech, precedingSurface)});
    }

    detail::appendSyntheticReadingCandidates(
        *candidates,
        candidateIndices,
        reading,
        precedingPartOfSpeech,
        precedingSurface);
    detail::sortAndTrim(*candidates, 8);
    return candidates;
}

std::shared_ptr<const Dictionary::CandidateVector> Dictionary::buildPredictionCandidates(
    ReadingView readingPrefix,
    size_t limit,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface) const {
    auto candidates = std::make_shared<CandidateVector>();
    candidates->reserve(limit + 1);
    std::unordered_map<detail::CandidateKey, size_t, detail::CandidateKeyHash> candidateIndices;
    candidateIndices.reserve(limit + 2);

    std::vector<EntryIndex> predictionIndices;
    predictionIndices.reserve(limit);
    defaultDb().findPredictions(readingPrefix, limit, predictionIndices);
    for (const EntryIndex index : predictionIndices) {
        const auto& entry = defaultDb().entryAt(index);
        detail::appendEntryCandidate(
                *candidates,
                candidateIndices,
                entry,
                blackDb,
                detail::predictionLengthPenalty(entry.reading.size(), readingPrefix.size()) +
                    detail::exactPredictionBoost(entry.reading.size(), readingPrefix.size()) +
                    detail::contextualPartOfSpeechBoost(entry, precedingPartOfSpeech) +
                    detail::surfaceGrammarBoost(entry, precedingPartOfSpeech, precedingSurface) +
                    detail::lexicalCategoryBoost(entry, precedingPartOfSpeech) +
                    userDb.contextScore(precedingSurface, entry.reading, entry.text));
        }

    predictionIndices.clear();
    userDb.findPredictions(readingPrefix, limit, predictionIndices);
    for (const EntryIndex index : predictionIndices) {
        const auto& entry = userDb.entryAt(index);
        detail::appendEntryCandidate(
                *candidates,
                candidateIndices,
                entry,
                blackDb,
                detail::predictionLengthPenalty(entry.reading.size(), readingPrefix.size()) +
                    detail::exactPredictionBoost(entry.reading.size(), readingPrefix.size()) +
                    detail::contextualPartOfSpeechBoost(entry, precedingPartOfSpeech) +
                    detail::surfaceGrammarBoost(entry, precedingPartOfSpeech, precedingSurface) +
                    detail::lexicalCategoryBoost(entry, precedingPartOfSpeech) +
                    userDb.contextScore(precedingSurface, entry.reading, entry.text));
        }

    if (!blackDb.isBlocked(readingPrefix, readingPrefix)) {
        const std::u16string raw(readingPrefix);
        detail::appendUniqueCandidate(
            *candidates,
            candidateIndices,
            {raw,
             raw,
             detail::kRawPredictionScore +
                 detail::rawReadingGrammarBoost(readingPrefix, precedingPartOfSpeech, precedingSurface)});
    }

    detail::appendSyntheticReadingCandidates(
        *candidates,
        candidateIndices,
        readingPrefix,
        precedingPartOfSpeech,
        precedingSurface);
    detail::sortAndTrim(*candidates, limit);
    return candidates;
}

}
