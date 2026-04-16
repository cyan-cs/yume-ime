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

#include <cstdint>
#include <optional>

namespace yume::ime::input {

    // Abstract Key Codes to completely decouple from OS Virtual Keys (VK_*)
    enum class KeyCode : uint32_t {
        Unknown = 0,
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        ToggleIme,
        ModeLatin,
        ModeHiragana,
        Space,
        Enter,
        Tab,
        Backspace,
        Delete,
        Escape,
        Left, Right, Up, Down,
        Home, End,
        PageUp, PageDown,
        Shift, Ctrl, Alt
        // Extend as necessary
    };

    // Abstract representation of a key event completely decoupled from Windows Virtual Keys
    struct KeyEvent {
        KeyCode keyCode;                   // The standardized abstract key code
        std::optional<char16_t> character; // The character if the key translates to a printable char (empty for Shift, etc.)
        bool isKeyDown;                    // True if key pushed down, false if released
        
        // Modifiers
        bool shiftPressed;
        bool ctrlPressed;
        bool altPressed;

        // Constructors
        KeyEvent() 
            : keyCode(KeyCode::Unknown), isKeyDown(false), 
              shiftPressed(false), ctrlPressed(false), altPressed(false) {}

        KeyEvent(KeyCode code, std::optional<char16_t> ch, bool down, bool shift, bool ctrl, bool alt)
            : keyCode(code), character(ch), isKeyDown(down),
              shiftPressed(shift), ctrlPressed(ctrl), altPressed(alt) {}
    };

}
