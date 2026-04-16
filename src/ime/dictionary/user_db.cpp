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


#include "ime/dictionary/user_db.hpp"

#include "ime/dictionary/db_binary_io.hpp"
#include "ime/dictionary/db_types.hpp"
#include "utils/logger.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace yume::ime::dictionary {

namespace {

constexpr uint32_t kUserDbMagicV2 = 0x55444232;
constexpr EntryScore kMaxScore = 2000000000;
constexpr int32_t kMaxContextCount = 1000000;
constexpr int32_t kContextBoostStep = 160;
constexpr int32_t kMaxContextBoost = 640;

bool isBetterEntry(const LexiconEntry& lhs, const LexiconEntry& rhs) {
    if (lhs.score != rhs.score) {
        return lhs.score > rhs.score;
    }
    if (lhs.reading.size() != rhs.reading.size()) {
        return lhs.reading.size() < rhs.reading.size();
    }
    return lhs.text < rhs.text;
}

bool readContextEntry(
    std::ifstream& input,
    std::u16string& previousText,
    std::u16string& reading,
    std::u16string& text,
    int32_t& storedCount) {
    if (!binary_io::readString(input, previousText) ||
        !binary_io::readString(input, reading) ||
        !binary_io::readString(input, text)) {
        return false;
    }

    input.read(reinterpret_cast<char*>(&storedCount), sizeof(storedCount));
    return input.good();
}

bool writeContextEntry(
    std::ofstream& output,
    std::u16string_view previousText,
    std::u16string_view reading,
    std::u16string_view text,
    int32_t count) {
    if (!binary_io::writeString(output, previousText) ||
        !binary_io::writeString(output, reading) ||
        !binary_io::writeString(output, text)) {
        return false;
    }

    output.write(reinterpret_cast<const char*>(&count), sizeof(count));
    return output.good();
}

}
size_t UserDb::ContextKeyHash::operator()(const ContextKey& key) const {
    auto seed = std::hash<std::u16string>{}(key.previousText);
    seed ^= std::hash<std::u16string>{}(key.reading) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    seed ^= std::hash<std::u16string>{}(key.text) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    return seed;
}

void UserDb::recordCommit(ReadingView reading, SurfaceView text) {
    if (reading.empty() || text.empty()) {
        return;
    }

    const EntryIndex entryIndex = ensureEntry(reading, text);
    auto& score = entries[static_cast<size_t>(entryIndex)].score;
    score = (score > kMaxScore - 1000) ? kMaxScore : static_cast<int32_t>(score + 1000);
    rebuildPath(reading);
    ++dbGeneration;
}

void UserDb::recordContextTransition(SurfaceView previousText, ReadingView reading, SurfaceView text) {
    if (previousText.empty() || reading.empty() || text.empty()) {
        return;
    }

    auto& count = contextCounts[ContextKey{
        std::u16string(previousText),
        std::u16string(reading),
        std::u16string(text),
    }];
    if (count < kMaxContextCount) {
        ++count;
    }
    ++dbGeneration;
}

bool UserDb::loadFromFile(const std::filesystem::path& path) {
    *this = UserDb{};
    UserDb loaded;

    const binary_io::PathState state = binary_io::pathStateNoThrow(path, "UserDb");
    if (state == binary_io::PathState::Missing) {
        *this = std::move(loaded);
        YUME_LOG_INFO("UserDb", "load skipped missing path=", path.string());
        return true;
    }
    if (state == binary_io::PathState::Error) {
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        YUME_LOG_ERROR("UserDb", "failed to open path=", path.string());
        return false;
    }

    uint32_t magic = 0;
    uint32_t count = 0;
    input.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    input.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!input.good() || count > binary_io::kMaxEntryCount || magic != kUserDbMagicV2) {
        YUME_LOG_ERROR("UserDb", "invalid header path=", path.string(), " count=", count);
        return false;
    }

    loaded.entries.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        std::u16string reading;
        std::u16string text;
        EntryScore score = 0;
        if (!binary_io::readString(input, reading) || !binary_io::readString(input, text)) {
            return false;
        }
        input.read(reinterpret_cast<char*>(&score), sizeof(score));
        if (!input.good()) {
            return false;
        }

        const EntryIndex entryIndex = loaded.ensureEntry(reading, text);
        loaded.entries[static_cast<size_t>(entryIndex)].score = score;
    }

    uint32_t contextCount = 0;
    input.read(reinterpret_cast<char*>(&contextCount), sizeof(contextCount));
    if (!input.good() || contextCount > binary_io::kMaxEntryCount) {
        return false;
    }

    for (uint32_t i = 0; i < contextCount; ++i) {
        std::u16string previousText;
        std::u16string reading;
        std::u16string text;
        int32_t storedCount = 0;
        if (!readContextEntry(input, previousText, reading, text, storedCount)) {
            return false;
        }

        loaded.contextCounts[ContextKey{
            std::move(previousText),
            std::move(reading),
            std::move(text),
        }] = storedCount;
    }

    loaded.rebuildAllPredictions();
    ++loaded.dbGeneration;
    *this = std::move(loaded);
    YUME_LOG_INFO("UserDb", "loaded entries=", entries.size(), " path=", path.string());
    return true;
}

bool UserDb::saveToFile(const std::filesystem::path& path) const {
    if (!binary_io::ensureParentDirectory(path)) {
        YUME_LOG_ERROR("UserDb", "failed to create parent directory path=", path.string());
        return false;
    }

    if (entries.size() > binary_io::kMaxEntryCount) {
        YUME_LOG_ERROR("UserDb", "too many entries to save path=", path.string(), " count=", entries.size());
        return false;
    }

    const std::filesystem::path tempPath = path.native() + L".tmp";
    std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        YUME_LOG_ERROR("UserDb", "failed to open temp for save path=", tempPath.string());
        return false;
    }

    if (!binary_io::writeEntryCount(output, kUserDbMagicV2, entries.size())) {
        output.close();
        binary_io::removeNoThrow(tempPath);
        return false;
    }

    for (const auto& entry : entries) {
        if (!binary_io::writeString(output, entry.reading) || !binary_io::writeString(output, entry.text)) {
            output.close();
            binary_io::removeNoThrow(tempPath);
            return false;
        }
        output.write(reinterpret_cast<const char*>(&entry.score), sizeof(entry.score));
        if (!output.good()) {
            output.close();
            binary_io::removeNoThrow(tempPath);
            return false;
        }
    }

    if (contextCounts.size() > binary_io::kMaxEntryCount) {
        output.close();
        binary_io::removeNoThrow(tempPath);
        return false;
    }

    const auto persistedContextCount = static_cast<uint32_t>(contextCounts.size());
    output.write(reinterpret_cast<const char*>(&persistedContextCount), sizeof(persistedContextCount));
    if (!output.good()) {
        output.close();
        binary_io::removeNoThrow(tempPath);
        return false;
    }

    for (const auto& [key, count] : contextCounts) {
        if (!writeContextEntry(output, key.previousText, key.reading, key.text, count)) {
            output.close();
            binary_io::removeNoThrow(tempPath);
            return false;
        }
    }

    if (!binary_io::finalizeAtomicSave(output, tempPath, path, "UserDb")) {
        return false;
    }
    YUME_LOG_INFO("UserDb", "saved entries=", entries.size(), " path=", path.string());
    return true;
}

const std::vector<EntryIndex>* UserDb::findExact(ReadingView reading) const {
    const TrieNode* node = findNode(reading);
    return (node && !node->exactEntryIndices.empty()) ? &node->exactEntryIndices : nullptr;
}

void UserDb::findPredictions(
    ReadingView prefix,
    size_t limit,
    std::vector<EntryIndex>& outPredictionIndices) const {
    outPredictionIndices.clear();
    const TrieNode* node = findNode(prefix);
    if (!node || node->topPredictionIndices.empty()) {
        return;
    }

    const size_t clampedLimit = (std::min)(limit, node->topPredictionIndices.size());
    outPredictionIndices.insert(
        outPredictionIndices.end(),
        node->topPredictionIndices.begin(),
        node->topPredictionIndices.begin() + static_cast<std::ptrdiff_t>(clampedLimit));
}

const LexiconEntry& UserDb::entryAt(EntryIndex index) const {
    return entries[static_cast<size_t>(index)];
}

bool UserDb::hasExactReading(ReadingView reading) const {
    const TrieNode* node = findNode(reading);
    return node != nullptr && !node->exactEntryIndices.empty();
}

int32_t UserDb::contextScore(SurfaceView previousText, ReadingView reading, SurfaceView text) const {
    if (previousText.empty() || reading.empty() || text.empty()) {
        return 0;
    }

    const auto it = contextCounts.find(ContextKey{
        std::u16string(previousText),
        std::u16string(reading),
        std::u16string(text),
    });
    if (it == contextCounts.end() || it->second <= 0) {
        return 0;
    }

    return (std::min)(it->second * kContextBoostStep, kMaxContextBoost);
}

EntryIndex UserDb::ensureEntry(ReadingView reading, SurfaceView text) {
    const std::u16string key = makeEntryKey(reading, text);
    const auto found = entryByKey.find(key);
    if (found != entryByKey.end()) {
        return found->second;
    }

    const EntryIndex entryIndex = static_cast<EntryIndex>(entries.size());
    entries.push_back({std::u16string(reading), std::u16string(text), 0});
    entryByKey.emplace(key, entryIndex);

    NodeIndex nodeIndex = 0;
    for (const char16_t ch : reading) {
        auto& children = nodes[static_cast<size_t>(nodeIndex)].children;
        const auto it = children.find(ch);
        if (it != children.end()) {
            nodeIndex = it->second;
        } else {
            const NodeIndex nextIndex = static_cast<NodeIndex>(nodes.size());
            children.emplace(ch, nextIndex);
            nodes.emplace_back();
            nodeIndex = nextIndex;
        }
    }
    nodes[static_cast<size_t>(nodeIndex)].exactEntryIndices.push_back(entryIndex);
    return entryIndex;
}

void UserDb::rebuildPath(ReadingView reading) {
    std::vector<NodeIndex> path = {0};
    NodeIndex nodeIndex = 0;

    for (const char16_t ch : reading) {
        const auto it = nodes[static_cast<size_t>(nodeIndex)].children.find(ch);
        if (it == nodes[static_cast<size_t>(nodeIndex)].children.end()) {
            return;
        }
        nodeIndex = it->second;
        path.push_back(nodeIndex);
    }

    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        recomputeTopPredictions(*it);
    }
}

void UserDb::rebuildAllPredictions() {
    if (nodes.empty()) {
        return;
    }

    for (NodeIndex nodeIndex = static_cast<NodeIndex>(nodes.size() - 1);; --nodeIndex) {
        recomputeTopPredictions(nodeIndex);
        if (nodeIndex == 0) {
            break;
        }
    }
}

void UserDb::recomputeTopPredictions(NodeIndex nodeIndex) {
    auto& node = nodes[static_cast<size_t>(nodeIndex)];
    std::vector<EntryIndex> merged = node.exactEntryIndices;

    for (const auto& [_, childIndex] : node.children) {
        const auto& childTop = nodes[static_cast<size_t>(childIndex)].topPredictionIndices;
        merged.insert(merged.end(), childTop.begin(), childTop.end());
    }

    std::sort(merged.begin(), merged.end(), [&](EntryIndex lhs, EntryIndex rhs) {
        return isBetterEntry(entries[static_cast<size_t>(lhs)], entries[static_cast<size_t>(rhs)]);
    });
    merged.erase(std::unique(merged.begin(), merged.end()), merged.end());
    if (merged.size() > kTopPredictionCount) {
        merged.resize(kTopPredictionCount);
    }
    node.topPredictionIndices = std::move(merged);
}

const UserDb::TrieNode* UserDb::findNode(ReadingView reading) const {
    NodeIndex nodeIndex = 0;
    for (const char16_t ch : reading) {
        const auto& children = nodes[static_cast<size_t>(nodeIndex)].children;
        const auto it = children.find(ch);
        if (it == children.end()) {
            return nullptr;
        }
        nodeIndex = it->second;
    }
    return &nodes[static_cast<size_t>(nodeIndex)];
}

}
