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

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace yume::ime::dictionary {

    class DefaultDb final {
    public:
        DefaultDb();

        const std::vector<EntryIndex>* findExact(ReadingView reading) const;
        void findPredictions(
            ReadingView readingPrefix,
            size_t limit,
            std::vector<EntryIndex>& outPredictionIndices) const;
        const LexiconEntry& entryAt(EntryIndex index) const;
        bool hasExactReading(ReadingView reading) const;
        void reloadIfChanged() const;

    private:
        struct TrieNode {
            std::unordered_map<char16_t, NodeIndex> children;
            std::vector<EntryIndex> exactEntryIndices;
            std::vector<EntryIndex> topPredictionIndices;
        };

        struct Storage {
            std::vector<LexiconEntry> entries;
            std::vector<TrieNode> nodes;
        };

        static constexpr size_t kTopPredictionCount = 8;

        mutable std::shared_ptr<const Storage> sharedStorage;
        mutable uint64_t loadedSignature = 0;

        const Storage& storage() const;
        void ensureLoaded() const;
        static std::shared_ptr<const Storage> acquireStorage(
            const std::filesystem::path& path,
            uint64_t signature);
        static Storage initializeStorage(const std::filesystem::path& path);
        static uint64_t computeSourceSignature(const std::filesystem::path& path);
        static void buildFallbackEntries(Storage& storage);
        static bool loadEntriesFromPath(Storage& storage, const std::filesystem::path& path);
        static bool loadEntriesFromFile(
            Storage& storage,
            const std::filesystem::path& path,
            const std::filesystem::path& lexiconRoot);
        static void buildTrie(Storage& storage);
        static void computeTopPredictions(Storage& storage);
        static const TrieNode* findNode(const Storage& storage, ReadingView reading);
    };

}
