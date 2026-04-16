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


#include "ime/composition/buffer.hpp"
#include "ime/composition/normalizer.hpp"

#include <cstddef>
#include <stdexcept>

namespace yume::ime::composition {

    void Buffer::insert(char16_t ch) {
        if (cursor > text.length()) {
            throw std::out_of_range("Buffer cursor is out of range");
        }

        const char16_t normalized = Normalizer::toHalfWidthChar(ch);
        if (cursor == text.length()) {
            if (text.size() == text.capacity()) {
                text.reserve(text.capacity() == 0 ? 8 : text.capacity() * 2);
            }
            text.push_back(normalized);
            ++cursor;
            return;
        }

        text.insert(text.begin() + static_cast<std::ptrdiff_t>(cursor), normalized);
        ++cursor;
    }

    void Buffer::backspace() {
        if (cursor > 0) {
            if (cursor == text.length()) {
                text.pop_back();
                --cursor;
                return;
            }

            text.erase(text.begin() + static_cast<std::ptrdiff_t>(cursor - 1));
            --cursor;
        }
    }

    void Buffer::deleteForward() {
        if (cursor >= text.length()) {
            return;
        }

        text.erase(text.begin() + static_cast<std::ptrdiff_t>(cursor));
    }

    bool Buffer::moveCursorLeft() {
        if (cursor == 0) {
            return false;
        }

        --cursor;
        return true;
    }

    bool Buffer::moveCursorRight() {
        if (cursor >= text.length()) {
            return false;
        }

        ++cursor;
        return true;
    }

    void Buffer::clear() {
        text.clear();
        cursor = 0;
    }

}
