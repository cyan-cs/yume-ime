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

#include <algorithm>
#include <iterator>

namespace yume::ime::dictionary {

namespace {

size_t toIndex(NodeIndex index) {
    return static_cast<size_t>(index);
}

EntryIndex toEntryIndex(size_t index) {
    return static_cast<EntryIndex>(index);
}

}
void DefaultDb::buildTrie(Storage& storage) {
    storage.nodes.clear();
    storage.nodes.emplace_back();

    for (size_t entryIndex = 0; entryIndex < storage.entries.size(); ++entryIndex) {
        NodeIndex nodeIndex = 0;
        for (const char16_t ch : storage.entries[entryIndex].reading) {
            auto& children = storage.nodes[toIndex(nodeIndex)].children;
            const auto it = children.find(ch);
            if (it != children.end()) {
                nodeIndex = it->second;
            } else {
                const NodeIndex nextIndex = static_cast<NodeIndex>(storage.nodes.size());
                children.emplace(ch, nextIndex);
                storage.nodes.emplace_back();
                nodeIndex = nextIndex;
            }
        }
        storage.nodes[toIndex(nodeIndex)].exactEntryIndices.push_back(toEntryIndex(entryIndex));
    }
}

void DefaultDb::computeTopPredictions(Storage& storage) {
    const auto visit = [&](const auto& self, NodeIndex nodeIndex) -> std::vector<EntryIndex> {
        auto& node = storage.nodes[toIndex(nodeIndex)];
        std::vector<EntryIndex> merged = node.exactEntryIndices;
        merged.reserve(node.exactEntryIndices.size() + node.children.size() * kTopPredictionCount);

        for (const auto& [_, childIndex] : node.children) {
            auto childEntries = self(self, childIndex);
            merged.insert(
                merged.end(),
                std::make_move_iterator(childEntries.begin()),
                std::make_move_iterator(childEntries.end()));
        }

        std::sort(merged.begin(), merged.end(), [&](EntryIndex lhs, EntryIndex rhs) {
            return detail::isBetterEntry(
                storage.entries[toIndex(lhs)],
                storage.entries[toIndex(rhs)]);
        });
        if (merged.size() > kTopPredictionCount) {
            merged.resize(kTopPredictionCount);
        }
        node.topPredictionIndices = merged;
        return merged;
    };

    visit(visit, 0);
}

const DefaultDb::TrieNode* DefaultDb::findNode(const Storage& storage, ReadingView reading) {
    NodeIndex nodeIndex = 0;
    for (const char16_t ch : reading) {
        const auto& children = storage.nodes[toIndex(nodeIndex)].children;
        const auto it = children.find(ch);
        if (it == children.end()) {
            return nullptr;
        }
        nodeIndex = it->second;
    }
    return &storage.nodes[toIndex(nodeIndex)];
}

}
