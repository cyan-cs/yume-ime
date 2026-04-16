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

namespace yume::ime::composition {

    // 逕溘・繧ｭ繝ｼ繧ｹ繝医Ο繝ｼ繧ｯ繧ｷ繝ｼ繧ｱ繝ｳ繧ｹ縺ｨ繧ｫ繝ｼ繧ｽ繝ｫ菴咲ｽｮ繧剃ｿ晄戟縺励∪縺・
    class Buffer {
    public:
        Buffer() : cursor(0) {}
        ~Buffer() = default;

        void insert(char16_t ch);
        void backspace();
        void deleteForward();
        bool moveCursorLeft();
        bool moveCursorRight();
        void clear();
        void reserve(size_t capacity) { text.reserve(capacity); }

        const std::u16string& getText() const { return text; }
        size_t getLength() const { return text.length(); }
        size_t getCursor() const { return cursor; }

    private:
        std::u16string text;
        size_t cursor;
    };

}
