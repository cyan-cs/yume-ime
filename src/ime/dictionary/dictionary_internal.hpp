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

#include "ime/dictionary/dictionary.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace yume::ime::dictionary::detail {

constexpr auto kMinUserDbFlushInterval = std::chrono::seconds(5);
constexpr auto kMaxUserDbFlushInterval = std::chrono::seconds(30);
constexpr int32_t kRawPredictionScore = 50;
constexpr int32_t kRomajiCandidateScore = -10;
constexpr int32_t kKatakanaCandidateScore = -20;

struct CandidateKey {
    std::u16string reading;
    std::u16string text;

    bool operator==(const CandidateKey& other) const {
        return reading == other.reading && text == other.text;
    }
};

struct CandidateKeyHash {
    size_t operator()(const CandidateKey& key) const;
};

struct FileSignature {
    bool exists = false;
    bool valid = true;
    uintmax_t size = 0;
    std::filesystem::file_time_type lastWriteTime{};

    bool operator==(const FileSignature& other) const;
};

struct CachedReloadState {
    FileSignature userDbSignature;
    FileSignature blackDbSignature;
    Dictionary::SessionSnapshot snapshot;
    bool userLoaded = false;
    bool blackLoaded = false;
};

std::string makeReloadCacheKey(const DictionaryStoragePaths& paths);
FileSignature readFileSignature(const std::filesystem::path& path);
bool tryLoadCachedReloadState(const DictionaryStoragePaths& paths, CachedReloadState& outState);
void storeCachedReloadState(const DictionaryStoragePaths& paths, const CachedReloadState& state);

int32_t predictionLengthPenalty(size_t readingLength, size_t prefixLength);
int32_t exactPredictionBoost(size_t readingLength, size_t prefixLength);
int32_t contextualPartOfSpeechBoost(const LexiconEntry& entry, PartOfSpeech precedingPartOfSpeech);
int32_t surfaceGrammarBoost(
    const LexiconEntry& entry,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface);
int32_t rawReadingGrammarBoost(ReadingView reading, PartOfSpeech precedingPartOfSpeech, SurfaceView precedingSurface);
int32_t syntheticReadingGrammarBoost(
    ReadingView reading,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface);
int32_t lexicalCategoryBoost(const LexiconEntry& entry, PartOfSpeech precedingPartOfSpeech);

void appendUniqueCandidate(
    Dictionary::CandidateVector& outCandidates,
    std::unordered_map<CandidateKey, size_t, CandidateKeyHash>& candidateIndices,
    const candidate::Candidate& candidate);
void sortAndTrim(Dictionary::CandidateVector& outCandidates, size_t limit);
void appendEntryCandidate(
    Dictionary::CandidateVector& outCandidates,
    std::unordered_map<CandidateKey, size_t, CandidateKeyHash>& candidateIndices,
    const LexiconEntry& entry,
    const BlackDb& blackDb,
    int32_t extraScore = 0);

std::u16string toKatakana(std::u16string_view reading);
std::u16string toRomaji(ReadingView reading);
void appendSyntheticReadingCandidates(
    Dictionary::CandidateVector& outCandidates,
    std::unordered_map<CandidateKey, size_t, CandidateKeyHash>& candidateIndices,
    ReadingView reading,
    PartOfSpeech precedingPartOfSpeech,
    SurfaceView precedingSurface);

bool recoverCorruptFile(const std::filesystem::path& path, std::string_view label);

}
