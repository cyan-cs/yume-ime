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

#include <string>

namespace yume::ime::composition {

// Handles text normalization rules (e.g., full-width to half-width).
class Normalizer {
public:
    // Converts full-width alphanumeric chars to half-width while preserving ASCII case.
    static std::u16string toHalfWidth(const std::u16string& input);

    // Normalize a single character to half-width while preserving ASCII case.
    static char16_t toHalfWidthChar(char16_t ch);

    // Normalize a single character to half-width and lowercase ASCII letters.
    static char16_t toHalfWidthLowerAsciiChar(char16_t ch);

    // Convert uppercase ASCII to lowercase natively.
    static char16_t toLowerAscii(char16_t ch);
};

}
