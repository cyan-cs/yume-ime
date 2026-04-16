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

#include "ime/input/key_event.hpp"
#include "ime/state/ime_states.hpp"
#include "platform/tsf/text_service_policy.hpp"

using namespace yume;

namespace {

ime::input::KeyEvent key(
    ime::input::KeyCode code,
    std::optional<char16_t> ch = std::nullopt,
    bool shift = false,
    bool ctrl = false,
    bool alt = false) {
    return ime::input::KeyEvent(code, ch, true, shift, ctrl, alt);
}

}

TEST(TextServicePolicyTest, CapsLockImeToggleIsDisabledByDefault) {
    platform::tsf::TextServiceConfig config;
    EXPECT_FALSE(platform::tsf::policy::shouldUseCapsLockAsImeToggle(config));
}

TEST(TextServicePolicyTest, CapsLockImeToggleCanBeEnabled) {
    platform::tsf::TextServiceConfig config;
    config.capsLockBehavior = platform::tsf::TextServiceConfig::CapsLockBehavior::ImeToggle;
    EXPECT_TRUE(platform::tsf::policy::shouldUseCapsLockAsImeToggle(config));
}

TEST(TextServicePolicyTest, FullWidthDirectInputRequiresDirectModeAndOption) {
    platform::tsf::TextServiceConfig config;
    config.enableFullWidthAlnum = true;

    EXPECT_TRUE(platform::tsf::policy::shouldCommitConfiguredFullWidthDirectInput(
        config,
        ime::state::ImeState::Direct,
        key(ime::input::KeyCode::A, u'a')));
    EXPECT_FALSE(platform::tsf::policy::shouldCommitConfiguredFullWidthDirectInput(
        config,
        ime::state::ImeState::Composing,
        key(ime::input::KeyCode::A, u'a')));
}

TEST(TextServicePolicyTest, FullWidthDirectInputRejectsModifiedAndNonAsciiKeys) {
    platform::tsf::TextServiceConfig config;
    config.enableFullWidthAlnum = true;

    EXPECT_FALSE(platform::tsf::policy::shouldCommitConfiguredFullWidthDirectInput(
        config,
        ime::state::ImeState::Direct,
        key(ime::input::KeyCode::A, u'a', false, true)));
    EXPECT_FALSE(platform::tsf::policy::shouldCommitConfiguredFullWidthDirectInput(
        config,
        ime::state::ImeState::Direct,
        key(ime::input::KeyCode::Unknown, u'!')));
}

TEST(TextServicePolicyTest, FullWidthDirectInputBuildsZenakuAscii) {
    platform::tsf::TextServiceConfig config;
    config.enableFullWidthAlnum = true;

    EXPECT_EQ(
        platform::tsf::policy::buildConfiguredDirectCommitText(
            config,
            ime::state::ImeState::Direct,
            key(ime::input::KeyCode::A, u'a')),
        u"\uFF41");
    EXPECT_EQ(
        platform::tsf::policy::buildConfiguredDirectCommitText(
            config,
            ime::state::ImeState::Direct,
            key(ime::input::KeyCode::A, u'A', true)),
        u"\uFF21");
}

TEST(TextServicePolicyTest, FullWidthDirectInputLeavesDigitsHalfWidth) {
    platform::tsf::TextServiceConfig config;
    config.enableFullWidthAlnum = true;

    EXPECT_FALSE(platform::tsf::policy::shouldCommitConfiguredFullWidthDirectInput(
        config,
        ime::state::ImeState::Direct,
        key(ime::input::KeyCode::Num1, u'1')));
    EXPECT_EQ(
        platform::tsf::policy::buildConfiguredDirectCommitText(
            config,
            ime::state::ImeState::Direct,
            key(ime::input::KeyCode::Num1, u'1')),
        u"");
}

TEST(TextServicePolicyTest, PrintableDuringConversionCommitsAndRestartsOnlyInConvertingState) {
    platform::tsf::TextServiceConfig config;

    EXPECT_TRUE(platform::tsf::policy::shouldCommitPrintableDuringConversion(
        config,
        ime::state::ImeState::Converting,
        key(ime::input::KeyCode::A, u'a')));
    EXPECT_FALSE(platform::tsf::policy::shouldCommitPrintableDuringConversion(
        config,
        ime::state::ImeState::Composing,
        key(ime::input::KeyCode::A, u'a')));
    EXPECT_FALSE(platform::tsf::policy::shouldCommitPrintableDuringConversion(
        config,
        ime::state::ImeState::Converting,
        key(ime::input::KeyCode::A, u'a', false, true)));
    EXPECT_FALSE(platform::tsf::policy::shouldCommitPrintableDuringConversion(
        config,
        ime::state::ImeState::Converting,
        key(ime::input::KeyCode::Space, u' ')));
}

TEST(TextServicePolicyTest, EscapeAndBackspaceCanCommitDuringConversionWhenConfigured) {
    platform::tsf::TextServiceConfig config;
    config.escapeInConversion = platform::tsf::TextServiceConfig::ConversionCancelBehavior::Commit;
    config.backspaceInConversion = platform::tsf::TextServiceConfig::ConversionCancelBehavior::Commit;

    EXPECT_TRUE(platform::tsf::policy::shouldCommitEscapeDuringConversion(
        config,
        ime::state::ImeState::Converting,
        key(ime::input::KeyCode::Escape)));
    EXPECT_TRUE(platform::tsf::policy::shouldCommitBackspaceDuringConversion(
        config,
        ime::state::ImeState::Converting,
        key(ime::input::KeyCode::Backspace)));
    EXPECT_FALSE(platform::tsf::policy::shouldCommitEscapeDuringConversion(
        config,
        ime::state::ImeState::Composing,
        key(ime::input::KeyCode::Escape)));
}

TEST(TextServicePolicyTest, FocusLossCommitsByDefault) {
    platform::tsf::TextServiceConfig config;
    EXPECT_TRUE(platform::tsf::policy::shouldCommitOnFocusLoss(config));

    config.focusLossBehavior = platform::tsf::TextServiceConfig::FocusLossBehavior::Keep;
    EXPECT_FALSE(platform::tsf::policy::shouldCommitOnFocusLoss(config));
}
