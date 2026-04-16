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



#include "ime/composition/converter.hpp"
#include "ime/composition/normalizer.hpp"
#include "ime/composition/romaji_table.hpp"

namespace yume::ime::composition {

namespace {

char16_t normalizeRomajiChar(char16_t ch) {
    return Normalizer::toHalfWidthLowerAsciiChar(ch);
}

void appendConvertedText(Converter::ConversionResult& result, std::u16string_view text) {
    if (result.composing.empty()) {
        result.confirmed.append(text);
        return;
    }

    result.composing.append(text);
}

void appendConvertedChar(Converter::ConversionResult& result, char16_t ch) {
    if (result.composing.empty()) {
        result.confirmed.push_back(ch);
        return;
    }

    result.composing.push_back(ch);
}

}

    Converter::ConversionResult Converter::convertRomajiToHiragana(const std::u16string& romaji) const {
        const std::u16string_view input{romaji};
        const size_t length = input.length();

        ConversionResult result;
        result.confirmed.reserve(length);
        result.composing.reserve(length);

        size_t index = 0;
        while (index < length) {
            if (handleSokuon(input, length, index, result) ||
                handleN(input, length, index, result) ||
                matchTrie(input, length, index, result)) {
                continue;
            }

            result.composing.push_back(input[index]);
            ++index;
        }

        return result;
    }

    bool Converter::isVowel(char16_t ch) {
        ch = normalizeRomajiChar(ch);
        return ch == u'a' || ch == u'i' || ch == u'u' || ch == u'e' || ch == u'o';
    }

    bool Converter::handleSokuon(std::u16string_view romaji, size_t length, size_t& index, ConversionResult& result) const {
        if (index + 1 >= length) {
            return false;
        }

        const char16_t current = normalizeRomajiChar(romaji[index]);
        const char16_t next = normalizeRomajiChar(romaji[index + 1]);
        if (current != next || current == u'n' || isVowel(current)) {
            return false;
        }

        appendConvertedChar(result, u'\u3063');
        ++index;
        return true;
    }

    bool Converter::handleN(std::u16string_view romaji, size_t length, size_t& index, ConversionResult& result) const {
        if (normalizeRomajiChar(romaji[index]) != u'n') {
            return false;
        }

        if (index + 1 >= length) {
            result.composing.push_back(u'n');
            ++index;
            return true;
        }

        const char16_t next = normalizeRomajiChar(romaji[index + 1]);
        if (next == u'n') {
            appendConvertedChar(result, u'\u3093');
            index += 2;
            return true;
        }

        if (next == u'y' || isVowel(next)) {
            return false;
        }

        if (next == u'\'') {
            appendConvertedChar(result, u'\u3093');
            index += 2;
            return true;
        }

        appendConvertedChar(result, u'\u3093');
        ++index;
        return true;
    }

    bool Converter::matchTrie(std::u16string_view romaji, size_t length, size_t& index, ConversionResult& result) const {
        const auto& nodes = RomajiTable::getNodes();
        int32_t currentIndex = 0;
        int32_t lastMatchIndex = TrieNode::kNoChild;
        size_t lastMatchLength = 0;
        size_t cursor = index;

        constexpr size_t kMaxLookahead = 4;
        while (cursor < length && (cursor - index) < kMaxLookahead) {
            const int slot = TrieNode::childIndex(normalizeRomajiChar(romaji[cursor]));
            if (slot < 0) {
                break;
            }

            currentIndex = nodes[static_cast<size_t>(currentIndex)].children[static_cast<size_t>(slot)];
            if (currentIndex == TrieNode::kNoChild) {
                break;
            }

            ++cursor;
            if (!nodes[static_cast<size_t>(currentIndex)].value.empty()) {
                lastMatchIndex = currentIndex;
                lastMatchLength = cursor - index;
            }
        }

        if (lastMatchIndex == TrieNode::kNoChild) {
            if (currentIndex != TrieNode::kNoChild && cursor == length) {
                result.composing.append(romaji.substr(index));
                index = length;
                return true;
            }
            return false;
        }

        appendConvertedText(result, nodes[static_cast<size_t>(lastMatchIndex)].value);
        index += lastMatchLength;
        return true;
    }

}
