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


#include "platform/tsf/text_service.hpp"

#include "platform/tsf/text_service_keymap_internal.hpp"
#include "platform/tsf/text_service_policy.hpp"

namespace yume::platform::tsf {

using ime::input::KeyCode;
using ime::input::KeyEvent;
using ime::state::ImeState;

bool TextService::shouldConsumeKey(WPARAM wParam, LPARAM lParam) const {
    const bool ctrlPressed = detail::isModifierPressed(VK_CONTROL);
    const bool altPressed = detail::isModifierPressed(VK_MENU);

    if (detail::isImeToggleVirtualKey(wParam) ||
        (shouldUseCapsLockAsImeToggle() && wParam == VK_CAPITAL) ||
        detail::isHiraganaModeVirtualKey(wParam) ||
        detail::isLatinModeVirtualKey(wParam) ||
        (ctrlPressed && !altPressed && wParam == VK_SPACE)) {
        return true;
    }

    const ImeState currentState = engine.getCurrentState();
    if (currentState == ImeState::Direct && config.enableFullWidthAlnum) {
        const bool shiftPressed = detail::isModifierPressed(VK_SHIFT);
        char16_t character = detail::kNoCharacter;
        if (detail::tryGetNormalizedPrintableCharacter(wParam, lParam, shiftPressed, character) &&
            policy::detail::isAsciiLetter(character)) {
            return true;
        }
    }
    if (currentState == ImeState::Direct || !engine.hasActiveSession()) {
        return false;
    }
    if (ctrlPressed || altPressed) {
        return false;
    }
    if (detail::isFastPrintableVirtualKey(wParam) || detail::isPrintableVirtualKey(wParam, lParam)) {
        return true;
    }

    const KeyCode keyCode = detail::mapVirtualKeyToKeyCode(wParam);
    switch (currentState) {
        case ImeState::Idle:
        case ImeState::Committed:
            return false;

        case ImeState::Composing:
            return keyCode == KeyCode::Escape || keyCode == KeyCode::Backspace || keyCode == KeyCode::Delete ||
                   keyCode == KeyCode::Left || keyCode == KeyCode::Right ||
                   keyCode == KeyCode::Up || keyCode == KeyCode::Down ||
                   keyCode == KeyCode::Enter || keyCode == KeyCode::Space ||
                   ((keyCode == KeyCode::Home || keyCode == KeyCode::End ||
                     keyCode == KeyCode::PageUp || keyCode == KeyCode::PageDown ||
                     keyCode == KeyCode::Tab) &&
                    engine.hasVisibleCandidateWindow()) ||
                   (keyCode >= KeyCode::Num0 && keyCode <= KeyCode::Num9);

        case ImeState::Converting:
            return keyCode == KeyCode::Enter || keyCode == KeyCode::Escape ||
                   keyCode == KeyCode::Backspace || keyCode == KeyCode::Space ||
                   keyCode == KeyCode::Left || keyCode == KeyCode::Right ||
                   keyCode == KeyCode::Up || keyCode == KeyCode::Down ||
                   keyCode == KeyCode::Home || keyCode == KeyCode::End ||
                   keyCode == KeyCode::PageUp || keyCode == KeyCode::PageDown ||
                   (keyCode >= KeyCode::Num0 && keyCode <= KeyCode::Num9);

        case ImeState::Direct:
        default:
            return false;
    }
}

bool TextService::buildKeyEvent(WPARAM wParam, LPARAM lParam, KeyEvent& event) const {
    if (detail::isImeToggleVirtualKey(wParam) || (shouldUseCapsLockAsImeToggle() && wParam == VK_CAPITAL)) {
        event.keyCode = KeyCode::ToggleIme;
        event.character = std::nullopt;
        event.isKeyDown = (lParam & (1LL << 31)) == 0;
        event.shiftPressed = detail::isModifierPressed(VK_SHIFT);
        event.ctrlPressed = false;
        event.altPressed = false;
        return true;
    }

    const bool shiftPressed = detail::isModifierPressed(VK_SHIFT);
    const bool ctrlPressed = detail::isModifierPressed(VK_CONTROL);
    const bool altPressed = detail::isModifierPressed(VK_MENU);
    const KeyCode keyCode = detail::mapVirtualKeyToKeyCode(wParam);

    char16_t character = detail::kNoCharacter;
    bool hasCharacter = false;
    if (!ctrlPressed && !altPressed) {
        hasCharacter = detail::tryGetNormalizedPrintableCharacter(wParam, lParam, shiftPressed, character);
    }
    if (keyCode == KeyCode::Unknown && !hasCharacter) {
        return false;
    }

    event.keyCode = keyCode;
    event.character = hasCharacter ? std::optional<char16_t>(character) : std::nullopt;
    event.isKeyDown = (lParam & (1LL << 31)) == 0;
    event.shiftPressed = shiftPressed;
    event.ctrlPressed = ctrlPressed;
    event.altPressed = altPressed;
    return true;
}

bool TextService::shouldUseCapsLockAsImeToggle() const {
    return policy::shouldUseCapsLockAsImeToggle(config);
}

bool TextService::shouldCommitConfiguredFullWidthDirectInput(const KeyEvent& event) const {
    return policy::shouldCommitConfiguredFullWidthDirectInput(config, engine.getCurrentState(), event);
}

std::u16string TextService::buildConfiguredDirectCommitText(const KeyEvent& event) const {
    return policy::buildConfiguredDirectCommitText(config, engine.getCurrentState(), event);
}

bool TextService::shouldCommitPrintableDuringConversion(const KeyEvent& event) const {
    return policy::shouldCommitPrintableDuringConversion(config, engine.getCurrentState(), event);
}

bool TextService::shouldCommitEscapeDuringConversion(const KeyEvent& event) const {
    return policy::shouldCommitEscapeDuringConversion(config, engine.getCurrentState(), event);
}

bool TextService::shouldCommitBackspaceDuringConversion(const KeyEvent& event) const {
    return policy::shouldCommitBackspaceDuringConversion(config, engine.getCurrentState(), event);
}

}
