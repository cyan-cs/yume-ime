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
#include <string_view>

namespace yume::ime::composition {

    class Converter {
    public:
        struct ConversionResult {
            std::u16string confirmed;
            std::u16string composing;
        };

        ConversionResult convertRomajiToHiragana(const std::u16string& romaji) const;

    private:
        static bool isVowel(char16_t ch);
        bool handleSokuon(std::u16string_view romaji, size_t length, size_t& index, ConversionResult& result) const;
        bool handleN(std::u16string_view romaji, size_t length, size_t& index, ConversionResult& result) const;
        bool matchTrie(std::u16string_view romaji, size_t length, size_t& index, ConversionResult& result) const;
    };

}
