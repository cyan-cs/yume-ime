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



#include "ime/dictionary/default_db.hpp"

#include "ime/dictionary/default_db_helpers.hpp"
#include "utils/app_paths.hpp"
#include "utils/logger.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_map>

namespace yume::ime::dictionary {

DefaultDb::DefaultDb() = default;

const std::vector<EntryIndex>* DefaultDb::findExact(ReadingView reading) const {
    const auto& loadedStorage = storage();
    const TrieNode* node = findNode(loadedStorage, reading);
    return (node && !node->exactEntryIndices.empty()) ? &node->exactEntryIndices : nullptr;
}

void DefaultDb::findPredictions(
    ReadingView readingPrefix,
    size_t limit,
    std::vector<EntryIndex>& outPredictionIndices) const {
    const auto& loadedStorage = storage();
    outPredictionIndices.clear();
    const TrieNode* node = findNode(loadedStorage, readingPrefix);
    if (!node || node->topPredictionIndices.empty()) {
        return;
    }

    const size_t clampedLimit = (std::min)(limit, node->topPredictionIndices.size());
    outPredictionIndices.insert(
        outPredictionIndices.end(),
        node->topPredictionIndices.begin(),
        node->topPredictionIndices.begin() + clampedLimit);
}

const LexiconEntry& DefaultDb::entryAt(EntryIndex index) const {
    const auto& loadedStorage = storage();
    return loadedStorage.entries[static_cast<size_t>(index)];
}

bool DefaultDb::hasExactReading(ReadingView reading) const {
    const auto& loadedStorage = storage();
    const TrieNode* node = findNode(loadedStorage, reading);
    return node != nullptr && !node->exactEntryIndices.empty();
}

const DefaultDb::Storage& DefaultDb::storage() const {
    ensureLoaded();
    return *sharedStorage;
}

void DefaultDb::ensureLoaded() const {
    reloadIfChanged();
}

void DefaultDb::reloadIfChanged() const {
    const auto path = yume::utils::paths::dataPath("dictionary/dictionary");
    const uint64_t currentSignature = computeSourceSignature(path);
    if (sharedStorage && loadedSignature == currentSignature) {
        return;
    }

    sharedStorage = acquireStorage(path, currentSignature);
    loadedSignature = currentSignature;
}

std::shared_ptr<const DefaultDb::Storage> DefaultDb::acquireStorage(
    const std::filesystem::path& path,
    uint64_t signature) {
    struct CacheEntry {
        uint64_t signature = 0;
        std::weak_ptr<const Storage> storage;
    };

    static std::mutex cacheMutex;
    static std::unordered_map<std::string, CacheEntry> cache;

    const std::string key = path.string();
    std::scoped_lock lock(cacheMutex);
    auto& entry = cache[key];
    if (entry.signature == signature) {
        if (auto existing = entry.storage.lock()) {
            return existing;
        }
    }

    auto created = std::make_shared<Storage>(initializeStorage(path));
    entry.signature = signature;
    entry.storage = created;
    return created;
}

DefaultDb::Storage DefaultDb::initializeStorage(const std::filesystem::path& path) {
    Storage storage;
    if (!loadEntriesFromPath(storage, path)) {
        YUME_LOG_WARN("DefaultDb", "using built-in fallback lexicon path=", path.string());
        buildFallbackEntries(storage);
    }
    buildTrie(storage);
    computeTopPredictions(storage);
    return storage;
}

uint64_t DefaultDb::computeSourceSignature(const std::filesystem::path& path) {
    std::error_code ec;
    uint64_t signature = 1469598103934665603ULL;

    const bool isFile = std::filesystem::is_regular_file(path, ec);
    if (ec) {
        return detail::hashCombine(signature, 1);
    }
    if (isFile) {
        signature = detail::hashCombine(signature, std::hash<std::string>{}(path.string()));
        signature = detail::hashCombine(signature, std::filesystem::file_size(path, ec));
        if (ec) {
            return detail::hashCombine(signature, 2);
        }
        const auto writeCount = std::filesystem::last_write_time(path, ec).time_since_epoch().count();
        if (ec) {
            return detail::hashCombine(signature, 3);
        }
        return detail::hashCombine(signature, static_cast<uint64_t>(writeCount));
    }

    const bool isDirectory = std::filesystem::is_directory(path, ec);
    if (ec || !isDirectory) {
        return detail::hashCombine(signature, 4);
    }

    std::vector<std::filesystem::path> lexiconFiles;
    for (std::filesystem::recursive_directory_iterator it(path, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".tsv") {
            lexiconFiles.push_back(it->path());
        }
    }
    if (ec) {
        return detail::hashCombine(signature, 5);
    }

    std::sort(lexiconFiles.begin(), lexiconFiles.end());
    for (const auto& lexiconFile : lexiconFiles) {
        const auto relative = std::filesystem::relative(lexiconFile, path, ec);
        signature = detail::hashCombine(signature, std::hash<std::string>{}(relative.string()));
        if (ec) {
            return detail::hashCombine(signature, 6);
        }
        signature = detail::hashCombine(signature, std::filesystem::file_size(lexiconFile, ec));
        if (ec) {
            return detail::hashCombine(signature, 7);
        }
        const auto writeCount = std::filesystem::last_write_time(lexiconFile, ec).time_since_epoch().count();
        if (ec) {
            return detail::hashCombine(signature, 8);
        }
        signature = detail::hashCombine(signature, static_cast<uint64_t>(writeCount));
    }

    return signature;
}

}
