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



#include "platform/tsf/text_service_keymap_internal.hpp"

#include "ime/composition/normalizer.hpp"

#include <array>
#include <iterator>

namespace yume::platform::tsf::detail {

namespace {

constexpr size_t kTranslatedBufferSize = 4;

bool tryTranslatePrintableCharacter(WPARAM wParam, LPARAM lParam, char16_t& character) {
    std::array<BYTE, 256> keyboardState{};
    if (!GetKeyboardState(keyboardState.data())) {
        return false;
    }

    std::array<WCHAR, kTranslatedBufferSize> translated{};
    UINT scanCode = (lParam >> 16) & 0xFFu;
    if ((lParam & 0x01000000) != 0) {
        scanCode |= 0xE000u;
    }
    const HKL keyboardLayout = GetKeyboardLayout(0);
    const int translatedLength = ToUnicodeEx(
        static_cast<UINT>(wParam),
        scanCode,
        keyboardState.data(),
        translated.data(),
        static_cast<int>(translated.size()),
        0,
        keyboardLayout);

    if (translatedLength > 0) {
        character = static_cast<char16_t>(translated.front());
        return true;
    }

    if (translatedLength < 0) {
        std::array<WCHAR, kTranslatedBufferSize> clearBuffer{};
        ToUnicodeEx(
            static_cast<UINT>(wParam),
            scanCode,
            keyboardState.data(),
            clearBuffer.data(),
            static_cast<int>(clearBuffer.size()),
            0,
            keyboardLayout);
    }

    return false;
}

bool tryMapPrintableCharacter(WPARAM wParam, bool shiftPressed, char16_t& character) {
    if (wParam >= 'A' && wParam <= 'Z') {
        character = static_cast<char16_t>(shiftPressed ? wParam : (wParam - 'A' + 'a'));
        return true;
    }

    if (wParam >= '0' && wParam <= '9') {
        static constexpr char16_t kShiftedDigits[] = {
            u')', u'!', u'@', u'#', u'$', u'%', u'^', u'&', u'*', u'(',
        };
        character = shiftPressed
            ? kShiftedDigits[static_cast<size_t>(wParam - '0')]
            : static_cast<char16_t>(wParam);
        return true;
    }

    switch (wParam) {
        case VK_OEM_MINUS: character = shiftPressed ? u'_' : u'-'; return true;
        case VK_OEM_PLUS: character = shiftPressed ? u'+' : u'='; return true;
        case VK_OEM_COMMA: character = shiftPressed ? u'<' : u','; return true;
        case VK_OEM_PERIOD: character = shiftPressed ? u'>' : u'.'; return true;
        case VK_OEM_1: character = shiftPressed ? u':' : u';'; return true;
        case VK_OEM_2: character = shiftPressed ? u'?' : u'/'; return true;
        case VK_OEM_3: character = shiftPressed ? u'~' : u'`'; return true;
        case VK_OEM_4: character = shiftPressed ? u'{' : u'['; return true;
        case VK_OEM_5: character = shiftPressed ? u'|' : u'\\'; return true;
        case VK_OEM_6: character = shiftPressed ? u'}' : u']'; return true;
        case VK_OEM_7: character = shiftPressed ? u'"' : u'\''; return true;
        default: return false;
    }
}

}

bool isModifierPressed(int virtualKey) {
    return (GetKeyState(virtualKey) & 0x8000) != 0;
}

bool isImeToggleVirtualKey(WPARAM wParam) {
    return wParam == VK_OEM_3 || wParam == VK_KANJI || wParam == VK_OEM_AUTO || wParam == VK_OEM_ENLW;
}

bool isLatinModeVirtualKey(WPARAM wParam) {
    if (wParam == VK_IME_OFF) {
        return true;
    }
#ifdef VK_DBE_ALPHANUMERIC
    if (wParam == VK_DBE_ALPHANUMERIC) {
        return true;
    }
#endif
#ifdef VK_DBE_SBCSCHAR
    if (wParam == VK_DBE_SBCSCHAR) {
        return true;
    }
#endif
    return false;
}

bool isHiraganaModeVirtualKey(WPARAM wParam) {
    if (wParam == VK_KANA || wParam == VK_IME_ON) {
        return true;
    }
#ifdef VK_DBE_HIRAGANA
    if (wParam == VK_DBE_HIRAGANA) {
        return true;
    }
#endif
#ifdef VK_DBE_DBCSCHAR
    if (wParam == VK_DBE_DBCSCHAR) {
        return true;
    }
#endif
    return false;
}

bool isFastPrintableVirtualKey(WPARAM wParam) {
    if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= '0' && wParam <= '9')) {
        return true;
    }

    switch (wParam) {
        case VK_OEM_MINUS:
        case VK_OEM_PLUS:
        case VK_OEM_COMMA:
        case VK_OEM_PERIOD:
        case VK_OEM_1:
        case VK_OEM_2:
        case VK_OEM_3:
        case VK_OEM_4:
        case VK_OEM_5:
        case VK_OEM_6:
        case VK_OEM_7:
            return true;
        default:
            return false;
    }
}

bool tryGetNormalizedPrintableCharacter(WPARAM wParam, LPARAM lParam, bool shiftPressed, char16_t& character) {
    char16_t translated = kNoCharacter;
    if (tryMapPrintableCharacter(wParam, shiftPressed, translated) ||
        tryTranslatePrintableCharacter(wParam, lParam, translated)) {
        character = ime::composition::Normalizer::toHalfWidthChar(translated);
        return true;
    }
    return false;
}

yume::ime::input::KeyCode mapVirtualKeyToKeyCode(WPARAM wParam) {
    using yume::ime::input::KeyCode;

    if (wParam >= 'A' && wParam <= 'Z') {
        return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::A) + static_cast<uint32_t>(wParam - 'A'));
    }
    if (wParam >= '0' && wParam <= '9') {
        return static_cast<KeyCode>(static_cast<uint32_t>(KeyCode::Num0) + static_cast<uint32_t>(wParam - '0'));
    }

    switch (wParam) {
        case VK_SPACE: return KeyCode::Space;
        case VK_RETURN: return KeyCode::Enter;
        case VK_TAB: return KeyCode::Tab;
        case VK_BACK: return KeyCode::Backspace;
        case VK_DELETE: return KeyCode::Delete;
        case VK_ESCAPE: return KeyCode::Escape;
        case VK_LEFT: return KeyCode::Left;
        case VK_RIGHT: return KeyCode::Right;
        case VK_UP: return KeyCode::Up;
        case VK_DOWN: return KeyCode::Down;
        case VK_HOME: return KeyCode::Home;
        case VK_END: return KeyCode::End;
        case VK_PRIOR: return KeyCode::PageUp;
        case VK_NEXT: return KeyCode::PageDown;
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
            return KeyCode::Shift;
        case VK_KANA:
        case VK_IME_ON:
            return KeyCode::ModeHiragana;
#ifdef VK_DBE_HIRAGANA
        case VK_DBE_HIRAGANA:
            return KeyCode::ModeHiragana;
#endif
#ifdef VK_DBE_DBCSCHAR
        case VK_DBE_DBCSCHAR:
            return KeyCode::ModeHiragana;
#endif
        case VK_IME_OFF:
            return KeyCode::ModeLatin;
#ifdef VK_DBE_ALPHANUMERIC
        case VK_DBE_ALPHANUMERIC:
            return KeyCode::ModeLatin;
#endif
#ifdef VK_DBE_SBCSCHAR
        case VK_DBE_SBCSCHAR:
            return KeyCode::ModeLatin;
#endif
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
            return KeyCode::Ctrl;
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
            return KeyCode::Alt;
        default:
            return KeyCode::Unknown;
    }
}

bool isPrintableVirtualKey(WPARAM wParam, LPARAM lParam) {
    if (isFastPrintableVirtualKey(wParam)) {
        return true;
    }

    char16_t ignored = kNoCharacter;
    return tryGetNormalizedPrintableCharacter(wParam, lParam, false, ignored) ||
           tryGetNormalizedPrintableCharacter(wParam, lParam, true, ignored);
}

}
