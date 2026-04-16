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



#include "ime/dictionary/default_db_helpers.hpp"

#include <windows.h>

#include <string>

namespace yume::ime::dictionary::detail {

namespace {

std::u16string decodeTextToUtf16(UINT codePage, std::string_view text) {
    if (text.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(
        codePage,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (required <= 0) {
        return {};
    }

    std::wstring wide(static_cast<size_t>(required), L'\0');
    const int converted = MultiByteToWideChar(
        codePage,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        wide.data(),
        required);
    if (converted != required) {
        return {};
    }

    return std::u16string(wide.begin(), wide.end());
}

}
bool isBetterEntry(const LexiconEntry& lhs, const LexiconEntry& rhs) {
    if (lhs.score != rhs.score) {
        return lhs.score > rhs.score;
    }
    if (lhs.reading.size() != rhs.reading.size()) {
        return lhs.reading.size() < rhs.reading.size();
    }
    return lhs.text < rhs.text;
}

std::u16string decodeEntryText(std::string_view text) {
    auto decoded = decodeTextToUtf16(CP_UTF8, text);
    if (!decoded.empty()) {
        return decoded;
    }

    decoded = decodeTextToUtf16(CP_ACP, text);
    return decoded;
}

std::string trimAscii(std::string value) {
    const auto isSpace = [](unsigned char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    };

    size_t begin = 0;
    while (begin < value.size() && isSpace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && isSpace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

bool isLexiconWhitespace(char16_t ch) {
    return ch == u' ' || ch == u'\t' || ch == u'\r' || ch == u'\n' || ch == u'\u3000';
}

void trimLexiconWhitespace(std::u16string& value) {
    size_t begin = 0;
    while (begin < value.size() && isLexiconWhitespace(value[begin])) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && isLexiconWhitespace(value[end - 1])) {
        --end;
    }

    value = value.substr(begin, end - begin);
}

uint64_t hashCombine(uint64_t seed, uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    return seed;
}

}
