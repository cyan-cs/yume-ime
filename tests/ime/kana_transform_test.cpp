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



#include <gtest/gtest.h>

#include "ime/composition/kana_transform.hpp"

TEST(KanaTransformTest, ConvertsHiraganaToHalfWidthKatakana) {
    EXPECT_EQ(
        yume::ime::composition::kana::toHalfWidthKatakana(u"\u304D\u3087\u3046"),
        u"\uFF77\uFF6E\uFF73");
}

TEST(KanaTransformTest, PreservesKanjiWhileConvertingKana) {
    EXPECT_EQ(
        yume::ime::composition::kana::toHalfWidthKatakana(u"\u4ECA\u65E5\u306F"),
        u"\u4ECA\u65E5\uFF8A");
}

TEST(KanaTransformTest, ConvertsVoicedAndPunctuationForms) {
    EXPECT_EQ(
        yume::ime::composition::kana::toHalfWidthKatakana(u"\u304C\u3001\u3074\u30FC"),
        u"\uFF76\uFF9E\uFF64\uFF8B\uFF9F\uFF70");
}

TEST(KanaTransformTest, TracksOffsetExpansionForHalfWidthKatakana) {
    const auto result = yume::ime::composition::kana::transformToHalfWidthKatakana(u"\u304C\u304D");

    EXPECT_EQ(result.text, u"\uFF76\uFF9E\uFF77");
    EXPECT_EQ(result.sourceOffsets.size(), 3u);
    EXPECT_EQ(result.sourceOffsets[0], 0u);
    EXPECT_EQ(result.sourceOffsets[1], 2u);
    EXPECT_EQ(result.sourceOffsets[2], 3u);
}
