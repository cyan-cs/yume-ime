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

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace yume::ime::composition::kana {

struct TransformResult {
    std::u16string text;
    std::vector<size_t> sourceOffsets;
};

namespace detail {

inline char16_t hiraganaToKatakana(char16_t ch) {
    if (ch >= u'\u3041' && ch <= u'\u3096') {
        return static_cast<char16_t>(ch + 0x60);
    }
    return ch;
}

inline std::u16string_view fullWidthKatakanaToHalfWidth(char16_t ch) {
    switch (ch) {
        case u'\u3002': return u"\uFF61";
        case u'\u3001': return u"\uFF64";
        case u'\u30FB': return u"\uFF65";
        case u'\u30FC': return u"\uFF70";
        case u'\u300C': return u"\uFF62";
        case u'\u300D': return u"\uFF63";
        case u'\u30A1': return u"\uFF67";
        case u'\u30A2': return u"\uFF71";
        case u'\u30A3': return u"\uFF68";
        case u'\u30A4': return u"\uFF72";
        case u'\u30A5': return u"\uFF69";
        case u'\u30A6': return u"\uFF73";
        case u'\u30A7': return u"\uFF6A";
        case u'\u30A8': return u"\uFF74";
        case u'\u30A9': return u"\uFF6B";
        case u'\u30AA': return u"\uFF75";
        case u'\u30AB': return u"\uFF76";
        case u'\u30AC': return u"\uFF76\uFF9E";
        case u'\u30AD': return u"\uFF77";
        case u'\u30AE': return u"\uFF77\uFF9E";
        case u'\u30AF': return u"\uFF78";
        case u'\u30B0': return u"\uFF78\uFF9E";
        case u'\u30B1': return u"\uFF79";
        case u'\u30B2': return u"\uFF79\uFF9E";
        case u'\u30B3': return u"\uFF7A";
        case u'\u30B4': return u"\uFF7A\uFF9E";
        case u'\u30B5': return u"\uFF7B";
        case u'\u30B6': return u"\uFF7B\uFF9E";
        case u'\u30B7': return u"\uFF7C";
        case u'\u30B8': return u"\uFF7C\uFF9E";
        case u'\u30B9': return u"\uFF7D";
        case u'\u30BA': return u"\uFF7D\uFF9E";
        case u'\u30BB': return u"\uFF7E";
        case u'\u30BC': return u"\uFF7E\uFF9E";
        case u'\u30BD': return u"\uFF7F";
        case u'\u30BE': return u"\uFF7F\uFF9E";
        case u'\u30BF': return u"\uFF80";
        case u'\u30C0': return u"\uFF80\uFF9E";
        case u'\u30C1': return u"\uFF81";
        case u'\u30C2': return u"\uFF81\uFF9E";
        case u'\u30C3': return u"\uFF6F";
        case u'\u30C4': return u"\uFF82";
        case u'\u30C5': return u"\uFF82\uFF9E";
        case u'\u30C6': return u"\uFF83";
        case u'\u30C7': return u"\uFF83\uFF9E";
        case u'\u30C8': return u"\uFF84";
        case u'\u30C9': return u"\uFF84\uFF9E";
        case u'\u30CA': return u"\uFF85";
        case u'\u30CB': return u"\uFF86";
        case u'\u30CC': return u"\uFF87";
        case u'\u30CD': return u"\uFF88";
        case u'\u30CE': return u"\uFF89";
        case u'\u30CF': return u"\uFF8A";
        case u'\u30D0': return u"\uFF8A\uFF9E";
        case u'\u30D1': return u"\uFF8A\uFF9F";
        case u'\u30D2': return u"\uFF8B";
        case u'\u30D3': return u"\uFF8B\uFF9E";
        case u'\u30D4': return u"\uFF8B\uFF9F";
        case u'\u30D5': return u"\uFF8C";
        case u'\u30D6': return u"\uFF8C\uFF9E";
        case u'\u30D7': return u"\uFF8C\uFF9F";
        case u'\u30D8': return u"\uFF8D";
        case u'\u30D9': return u"\uFF8D\uFF9E";
        case u'\u30DA': return u"\uFF8D\uFF9F";
        case u'\u30DB': return u"\uFF8E";
        case u'\u30DC': return u"\uFF8E\uFF9E";
        case u'\u30DD': return u"\uFF8E\uFF9F";
        case u'\u30DE': return u"\uFF8F";
        case u'\u30DF': return u"\uFF90";
        case u'\u30E0': return u"\uFF91";
        case u'\u30E1': return u"\uFF92";
        case u'\u30E2': return u"\uFF93";
        case u'\u30E3': return u"\uFF6C";
        case u'\u30E4': return u"\uFF94";
        case u'\u30E5': return u"\uFF6D";
        case u'\u30E6': return u"\uFF95";
        case u'\u30E7': return u"\uFF6E";
        case u'\u30E8': return u"\uFF96";
        case u'\u30E9': return u"\uFF97";
        case u'\u30EA': return u"\uFF98";
        case u'\u30EB': return u"\uFF99";
        case u'\u30EC': return u"\uFF9A";
        case u'\u30ED': return u"\uFF9B";
        case u'\u30EE': return u"\uFF9C";
        case u'\u30EF': return u"\uFF9C";
        case u'\u30F2': return u"\uFF66";
        case u'\u30F3': return u"\uFF9D";
        case u'\u30F4': return u"\uFF73\uFF9E";
        case u'\u3099': return u"\uFF9E";
        case u'\u309A': return u"\uFF9F";
        default: return {};
    }
}

}

inline TransformResult transformToHalfWidthKatakana(std::u16string_view input) {
    TransformResult result;
    result.text.reserve(input.size() * 2);
    result.sourceOffsets.reserve(input.size() + 1);
    result.sourceOffsets.push_back(0);

    for (char16_t ch : input) {
        const char16_t katakana = detail::hiraganaToKatakana(ch);
        const auto mapped = detail::fullWidthKatakanaToHalfWidth(katakana);
        if (!mapped.empty()) {
            result.text.append(mapped);
        } else {
            result.text.push_back(ch);
        }
        result.sourceOffsets.push_back(result.text.size());
    }

    return result;
}

inline std::u16string toHalfWidthKatakana(std::u16string_view input) {
    return transformToHalfWidthKatakana(input).text;
}

}
