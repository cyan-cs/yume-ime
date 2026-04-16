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


#include "ime/composition/normalizer.hpp"

namespace yume::ime::composition {

    char16_t Normalizer::toHalfWidthChar(char16_t ch) {
        // 全角の ASCII 記号は半角へ正規化する。
        if (ch >= 0xFF01 && ch <= 0xFF5E) {
            ch = ch - 0xFEE0;
        } else if (ch == 0x3000) {
            ch = 0x0020;
        }

        return ch;
    }

    char16_t Normalizer::toHalfWidthLowerAsciiChar(char16_t ch) {
        return toLowerAscii(toHalfWidthChar(ch));
    }

    char16_t Normalizer::toLowerAscii(char16_t ch) {
        if (ch >= u'A' && ch <= u'Z') {
            return ch + (u'a' - u'A');
        }
        return ch;
    }

    std::u16string Normalizer::toHalfWidth(const std::u16string& input) {
        std::u16string result;
        result.reserve(input.length());
        for (char16_t ch : input) {
            result.push_back(toHalfWidthChar(ch));
        }
        return result;
    }

}
