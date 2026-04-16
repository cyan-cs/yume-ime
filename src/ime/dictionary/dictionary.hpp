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

#include "ime/dictionary/black_db.hpp"
#include "ime/dictionary/db_types.hpp"
#include "ime/dictionary/default_db.hpp"
#include "ime/dictionary/user_db.hpp"
#include "utils/app_paths.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace yume::ime::dictionary {

    struct DictionaryStoragePaths {
        std::filesystem::path userDbPath = yume::utils::paths::dataPath("dictionary/db/user_db.bin");
        std::filesystem::path blackDbPath = yume::utils::paths::dataPath("dictionary/db/black_db.bin");
    };

    class Dictionary final {
    public:
        using CandidateVector = std::vector<candidate::Candidate>;
        struct SessionSnapshot {
            UserDb userDb;
            BlackDb blackDb;
            bool isUserDbDirty = false;
            size_t pendingUserDbCommits = 0;
            uint64_t lastBlackDbSavedGeneration = 0;
            uint64_t contextGeneration = 0;
            PartOfSpeech lastCommittedPartOfSpeech = PartOfSpeech::Unknown;
            SurfaceText lastCommittedSurfaceText;
            bool userDbWritable = true;
            bool blackDbWritable = true;
            std::chrono::steady_clock::time_point lastUserDbCommitTime = std::chrono::steady_clock::now();
            std::chrono::steady_clock::time_point lastUserDbFlushTime = std::chrono::steady_clock::now();
        };

        struct CacheGeneration {
            uint64_t user = 0;
            uint64_t black = 0;
            uint64_t context = 0;

            bool operator==(const CacheGeneration& other) const {
                return user == other.user && black == other.black && context == other.context;
            }
        };

        Dictionary();
        explicit Dictionary(DictionaryStoragePaths paths);
        ~Dictionary();

        void getCandidates(
            const std::u16string& reading,
            CandidateVector& outCandidates,
            PartOfSpeech precedingPartOfSpeech = PartOfSpeech::Unknown,
            SurfaceView precedingSurface = SurfaceView{}) const;
        std::shared_ptr<const CandidateVector> getCandidatesShared(
            ReadingView reading,
            PartOfSpeech precedingPartOfSpeech = PartOfSpeech::Unknown,
            SurfaceView precedingSurface = SurfaceView{}) const;
        void getPredictions(
            ReadingView readingPrefix,
            CandidateVector& outCandidates,
            size_t limit = 8,
            PartOfSpeech precedingPartOfSpeech = PartOfSpeech::Unknown,
            SurfaceView precedingSurface = SurfaceView{}) const;
        std::shared_ptr<const CandidateVector> getPredictionsShared(
            ReadingView readingPrefix,
            size_t limit = 8,
            PartOfSpeech precedingPartOfSpeech = PartOfSpeech::Unknown,
            SurfaceView precedingSurface = SurfaceView{}) const;
        bool hasExactReading(ReadingView reading) const;
        PartOfSpeech lookupPartOfSpeech(ReadingView reading, SurfaceView text) const;
        PartOfSpeech lastCommittedPartOfSpeech() const;
        SurfaceText lastCommittedSurface() const;
        void recordCommit(const ReadingText& reading, const SurfaceText& text);
        void blockCandidate(ReadingView reading, SurfaceView text);
        void servicePersistence() const;
        bool reload();
        bool flush() const;
        SessionSnapshot captureSessionSnapshot() const;
        void restoreSessionSnapshot(const SessionSnapshot& snapshot);

    private:
        struct CachedCandidates {
            CacheGeneration generation;
            std::shared_ptr<const CandidateVector> items;
        };

        struct ExactCandidateCacheKey {
            std::u16string reading;
            PartOfSpeech precedingPartOfSpeech = PartOfSpeech::Unknown;
            std::u16string precedingSurface;

            bool operator==(const ExactCandidateCacheKey& other) const {
                return reading == other.reading &&
                       precedingPartOfSpeech == other.precedingPartOfSpeech &&
                       precedingSurface == other.precedingSurface;
            }
        };

        struct ExactCandidateCacheKeyHash {
            size_t operator()(const ExactCandidateCacheKey& key) const {
                size_t seed = std::hash<std::u16string>{}(key.reading);
                seed ^= std::hash<int>{}(static_cast<int>(key.precedingPartOfSpeech)) +
                        0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
                seed ^= std::hash<std::u16string>{}(key.precedingSurface) +
                        0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
                return seed;
            }
        };

        struct PredictionCacheKey {
            std::u16string reading;
            size_t limit = 0;
            PartOfSpeech precedingPartOfSpeech = PartOfSpeech::Unknown;
            std::u16string precedingSurface;

            bool operator==(const PredictionCacheKey& other) const {
                return reading == other.reading &&
                       limit == other.limit &&
                       precedingPartOfSpeech == other.precedingPartOfSpeech &&
                       precedingSurface == other.precedingSurface;
            }
        };

        struct PredictionCacheKeyHash {
            size_t operator()(const PredictionCacheKey& key) const {
                size_t seed = std::hash<std::u16string>{}(key.reading);
                seed ^= std::hash<size_t>{}(key.limit) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
                seed ^= std::hash<int>{}(static_cast<int>(key.precedingPartOfSpeech)) +
                        0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
                seed ^= std::hash<std::u16string>{}(key.precedingSurface) +
                        0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
                return seed;
            }
        };

        mutable std::mutex mutex;
        mutable std::mutex blackDbSaveMutex;
        DictionaryStoragePaths storagePaths;
        UserDb userDb;
        BlackDb blackDb;
        mutable std::unordered_map<ExactCandidateCacheKey, CachedCandidates, ExactCandidateCacheKeyHash>
            exactCandidateCache;
        mutable std::unordered_map<PredictionCacheKey, CachedCandidates, PredictionCacheKeyHash> predictionCache;
        mutable bool isUserDbDirty = false;
        mutable size_t pendingUserDbCommits = 0;
        mutable uint64_t lastBlackDbSavedGeneration = 0;
        mutable uint64_t contextGeneration = 0;
        mutable PartOfSpeech lastCommittedPartOfSpeechValue = PartOfSpeech::Unknown;
        mutable SurfaceText lastCommittedSurfaceText;
        mutable bool userDbWritable = true;
        mutable bool blackDbWritable = true;
        mutable std::chrono::steady_clock::time_point lastUserDbCommitTime = std::chrono::steady_clock::now();
        mutable std::chrono::steady_clock::time_point lastUserDbFlushTime = std::chrono::steady_clock::now();

        void clearCaches() const;
        std::chrono::steady_clock::duration currentUserDbFlushInterval() const;
        void flushUserDbIfDue() const;
        bool flushUserDbNow() const;
        CacheGeneration combinedGeneration() const;
        const DefaultDb& defaultDb() const;
        std::shared_ptr<const CandidateVector> buildExactCandidates(
            ReadingView reading,
            PartOfSpeech precedingPartOfSpeech,
            SurfaceView precedingSurface) const;
        std::shared_ptr<const CandidateVector> buildPredictionCandidates(
            ReadingView readingPrefix,
            size_t limit,
            PartOfSpeech precedingPartOfSpeech,
            SurfaceView precedingSurface) const;
    };

}
