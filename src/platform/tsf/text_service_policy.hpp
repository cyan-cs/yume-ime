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

#include "ime/input/key_event.hpp"
#include "ime/state/ime_states.hpp"
#include "platform/tsf/text_service_config.hpp"

#include <string>

namespace yume::platform::tsf::policy {

namespace detail {

inline bool isAsciiLetter(char16_t ch) {
    return (ch >= u'a' && ch <= u'z') ||
           (ch >= u'A' && ch <= u'Z');
}

}

inline bool shouldUseCapsLockAsImeToggle(const TextServiceConfig& config) {
    return config.usesImeToggleCapsLock();
}

inline bool shouldCommitConfiguredFullWidthDirectInput(
    const TextServiceConfig& config,
    ime::state::ImeState currentState,
    const ime::input::KeyEvent& event) {
    return currentState == ime::state::ImeState::Direct &&
           config.usesFullWidthDirectInput() &&
           event.isKeyDown &&
           !event.ctrlPressed &&
           !event.altPressed &&
           event.character.has_value() &&
           detail::isAsciiLetter(*event.character);
}

inline std::u16string buildConfiguredDirectCommitText(
    const TextServiceConfig& config,
    ime::state::ImeState currentState,
    const ime::input::KeyEvent& event) {
    if (!shouldCommitConfiguredFullWidthDirectInput(config, currentState, event)) {
        return {};
    }

    const char16_t ch = *event.character;
    return std::u16string(1, static_cast<char16_t>(ch + 0xFEE0));
}

inline bool shouldCommitPrintableDuringConversion(
    const TextServiceConfig& config,
    ime::state::ImeState currentState,
    const ime::input::KeyEvent& event) {
    return config.commitsPrintableDuringConversion() &&
           event.isKeyDown &&
           event.character.has_value() &&
           event.keyCode != ime::input::KeyCode::Space &&
           !event.ctrlPressed &&
           !event.altPressed &&
           currentState == ime::state::ImeState::Converting;
}

inline bool shouldCommitEscapeDuringConversion(
    const TextServiceConfig& config,
    ime::state::ImeState currentState,
    const ime::input::KeyEvent& event) {
    return currentState == ime::state::ImeState::Converting &&
           event.isKeyDown &&
           event.keyCode == ime::input::KeyCode::Escape &&
           config.commitsEscapeDuringConversion();
}

inline bool shouldCommitBackspaceDuringConversion(
    const TextServiceConfig& config,
    ime::state::ImeState currentState,
    const ime::input::KeyEvent& event) {
    return currentState == ime::state::ImeState::Converting &&
           event.isKeyDown &&
           event.keyCode == ime::input::KeyCode::Backspace &&
           config.commitsBackspaceDuringConversion();
}

inline bool shouldCommitOnFocusLoss(const TextServiceConfig& config) {
    return config.commitsOnFocusLoss();
}

}
