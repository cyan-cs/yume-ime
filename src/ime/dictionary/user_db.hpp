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

#include "ime/dictionary/db_types.hpp"

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace yume::ime::dictionary {

    class UserDb final {
    public:
        UserDb() = default;

        void recordCommit(ReadingView reading, SurfaceView text);
        void recordContextTransition(SurfaceView previousText, ReadingView reading, SurfaceView text);
        bool loadFromFile(const std::filesystem::path& path);
        bool saveToFile(const std::filesystem::path& path) const;
        const std::vector<EntryIndex>* findExact(ReadingView reading) const;
        void findPredictions(
            ReadingView prefix,
            size_t limit,
            std::vector<EntryIndex>& outPredictionIndices) const;
        const LexiconEntry& entryAt(EntryIndex index) const;
        bool hasExactReading(ReadingView reading) const;
        int32_t contextScore(SurfaceView previousText, ReadingView reading, SurfaceView text) const;
        uint64_t generation() const { return dbGeneration; }

    private:
        struct ContextKey {
            std::u16string previousText;
            std::u16string reading;
            std::u16string text;

            bool operator==(const ContextKey& other) const {
                return previousText == other.previousText &&
                       reading == other.reading &&
                       text == other.text;
            }
        };

        struct ContextKeyHash {
            size_t operator()(const ContextKey& key) const;
        };

        struct TrieNode {
            std::unordered_map<char16_t, NodeIndex> children;
            std::vector<EntryIndex> exactEntryIndices;
            std::vector<EntryIndex> topPredictionIndices;
        };

        static constexpr size_t kTopPredictionCount = 8;

        std::vector<LexiconEntry> entries;
        std::unordered_map<std::u16string, EntryIndex> entryByKey;
        std::vector<TrieNode> nodes = {TrieNode{}};
        std::unordered_map<ContextKey, int32_t, ContextKeyHash> contextCounts;
        uint64_t dbGeneration = 0;

        EntryIndex ensureEntry(ReadingView reading, SurfaceView text);
        void rebuildAllPredictions();
        void rebuildPath(ReadingView reading);
        void recomputeTopPredictions(NodeIndex nodeIndex);
        const TrieNode* findNode(ReadingView reading) const;
    };

}
