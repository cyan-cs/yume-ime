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

#include "platform/tsf/text_service_config.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>

using namespace yume::platform::tsf;

namespace {

class ScopedConfigDirCleanup {
public:
    ~ScopedConfigDirCleanup() {
        std::error_code ec;
        std::filesystem::remove_all(std::filesystem::path("build") / "gtest_config", ec);
    }
};

std::filesystem::path writeConfig(const char* name, const char* json) {
    const auto dir = std::filesystem::path("build") / "gtest_config";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        throw std::runtime_error("failed to prepare text service config test directory");
    }
    const auto path = dir / name;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open text service config test file");
    }
    output << json;
    if (!output.good()) {
        throw std::runtime_error("failed to write text service config test file");
    }
    return path;
}

}
TEST(TextServiceConfigTest, MissingFileReturnsDefaults) {
    const auto config = TextServiceConfig::loadFromFile("build/does_not_exist.json");

    EXPECT_EQ(config.compatibilityStyle, TextServiceConfig::CompatibilityStyle::ModernJp);
    EXPECT_EQ(config.capsLockBehavior, TextServiceConfig::CapsLockBehavior::System);
    EXPECT_EQ(
        config.printableDuringConversion,
        TextServiceConfig::PrintableDuringConversionBehavior::CommitAndInsert);
    EXPECT_EQ(config.focusLossBehavior, TextServiceConfig::FocusLossBehavior::Commit);
    EXPECT_EQ(config.compatibilityStyleName(), TextServiceConfig::kDefaultCompatibilityStyle);
    EXPECT_EQ(config.capsLockBehaviorName(), TextServiceConfig::kDefaultCapsLockBehavior);
    EXPECT_EQ(
        config.printableDuringConversionName(),
        TextServiceConfig::kDefaultPrintableDuringConversion);
    EXPECT_EQ(config.focusLossBehaviorName(), TextServiceConfig::kDefaultFocusLossBehavior);
    EXPECT_FALSE(config.enableFullWidthAlnum);
    EXPECT_FALSE(config.usesKatakanaOutput());
}

TEST(TextServiceConfigTest, LoaderNormalizesSupportedValues) {
    ScopedConfigDirCleanup cleanup;
    const auto path = writeConfig(
        "normalized.json",
        "{\n"
        "  \"compatibility_style\": \"MODERN_JP\",\n"
        "  \"caps_lock_behavior\": \"IME_TOGGLE\",\n"
        "  \"printable_during_conversion\": \"COMMIT_AND_INSERT\",\n"
        "  \"focus_loss_behavior\": \"KEEP\",\n"
        "  \"escape_in_conversion\": \"COMMIT\",\n"
        "  \"backspace_in_conversion\": \"COMMIT\",\n"
        "  \"enable_half_katakana\": true,\n"
        "  \"enable_full_width_alnum\": true\n"
        "}\n");

    const auto config = TextServiceConfig::loadFromFile(path);

    EXPECT_EQ(config.compatibilityStyle, TextServiceConfig::CompatibilityStyle::ModernJp);
    EXPECT_EQ(config.capsLockBehavior, TextServiceConfig::CapsLockBehavior::ImeToggle);
    EXPECT_EQ(
        config.printableDuringConversion,
        TextServiceConfig::PrintableDuringConversionBehavior::CommitAndInsert);
    EXPECT_EQ(config.focusLossBehavior, TextServiceConfig::FocusLossBehavior::Keep);
    EXPECT_EQ(config.escapeInConversion, TextServiceConfig::ConversionCancelBehavior::Commit);
    EXPECT_EQ(config.backspaceInConversion, TextServiceConfig::ConversionCancelBehavior::Commit);
    EXPECT_EQ(config.capsLockBehaviorName(), "ime_toggle");
    EXPECT_EQ(config.focusLossBehaviorName(), "keep");
    EXPECT_EQ(config.escapeInConversionName(), TextServiceConfig::kCommitConversionBehavior);
    EXPECT_EQ(config.backspaceInConversionName(), TextServiceConfig::kCommitConversionBehavior);
    EXPECT_TRUE(config.enableKatakana);
    EXPECT_TRUE(config.usesHalfKatakanaOutput());
    EXPECT_TRUE(config.enableFullWidthAlnum);
}

TEST(TextServiceConfigTest, LoaderFallsBackForUnsupportedValues) {
    ScopedConfigDirCleanup cleanup;
    const auto path = writeConfig(
        "fallback.json",
        "{\n"
        "  \"compatibility_style\": \"legacy\",\n"
        "  \"caps_lock_behavior\": \"weird\",\n"
        "  \"printable_during_conversion\": \"ignore\",\n"
        "  \"focus_loss_behavior\": \"blur\",\n"
        "  \"escape_in_conversion\": \"noop\",\n"
        "  \"backspace_in_conversion\": \"noop\"\n"
        "}\n");

    const auto config = TextServiceConfig::loadFromFile(path);

    EXPECT_EQ(config.compatibilityStyle, TextServiceConfig::CompatibilityStyle::ModernJp);
    EXPECT_EQ(config.capsLockBehavior, TextServiceConfig::CapsLockBehavior::System);
    EXPECT_EQ(
        config.printableDuringConversion,
        TextServiceConfig::PrintableDuringConversionBehavior::CommitAndInsert);
    EXPECT_EQ(config.focusLossBehavior, TextServiceConfig::FocusLossBehavior::Commit);
    EXPECT_EQ(
        config.escapeInConversion,
        TextServiceConfig::ConversionCancelBehavior::CancelToComposing);
    EXPECT_EQ(
        config.backspaceInConversion,
        TextServiceConfig::ConversionCancelBehavior::CancelToComposing);
}

TEST(TextServiceConfigTest, BoolFlagsOnlyChangeWhenValidBoolExists) {
    ScopedConfigDirCleanup cleanup;
    const auto path = writeConfig(
        "bools.json",
        "{\n"
        "  \"enable_katakana\": true,\n"
        "  \"enable_half_katakana\": \"yes\",\n"
        "  \"enable_full_width_alnum\": false\n"
        "}\n");

    const auto config = TextServiceConfig::loadFromFile(path);

    EXPECT_TRUE(config.enableKatakana);
    EXPECT_FALSE(config.enableHalfKatakana);
    EXPECT_FALSE(config.enableFullWidthAlnum);
    EXPECT_TRUE(config.usesKatakanaOutput());
    EXPECT_FALSE(config.usesHalfKatakanaOutput());
}

TEST(TextServiceConfigTest, LoaderAcceptsUnicodeEscapeSequences) {
    ScopedConfigDirCleanup cleanup;
    const auto path = writeConfig(
        "escaped.json",
        "{\n"
        "  \"compatibility_style\": \"MODERN_JP\",\n"
        "  \"focus_loss_behavior\": \"KEEP\",\n"
        "  \"printable_during_conversion\": \"COMMIT_AND_INSERT\",\n"
        "  \"caps_lock_behavior\": \"IME\\u005fTOGGLE\"\n"
        "}\n");

    const auto config = TextServiceConfig::loadFromFile(path);

    EXPECT_EQ(config.compatibilityStyle, TextServiceConfig::CompatibilityStyle::ModernJp);
    EXPECT_EQ(config.focusLossBehavior, TextServiceConfig::FocusLossBehavior::Keep);
    EXPECT_EQ(
        config.printableDuringConversion,
        TextServiceConfig::PrintableDuringConversionBehavior::CommitAndInsert);
    EXPECT_EQ(config.capsLockBehavior, TextServiceConfig::CapsLockBehavior::ImeToggle);
}

TEST(TextServiceConfigTest, LoaderRejectsMalformedStringAndBoolValues) {
    ScopedConfigDirCleanup cleanup;
    const auto path = writeConfig(
        "malformed.json",
        "{\n"
        "  \"caps_lock_behavior\": \"IME_TOGGLE,\n"
        "  \"enable_full_width_alnum\": truthy,\n"
        "  \"focus_loss_behavior\": \"KEEP\"\n"
        "}\n");

    const auto config = TextServiceConfig::loadFromFile(path);

    EXPECT_EQ(config.capsLockBehavior, TextServiceConfig::CapsLockBehavior::System);
    EXPECT_FALSE(config.enableFullWidthAlnum);
    EXPECT_TRUE(config.commitsOnFocusLoss());
}

TEST(TextServiceConfigTest, LoaderIgnoresKeyLikeTextInsideJsonStrings) {
    ScopedConfigDirCleanup cleanup;
    const auto path = writeConfig(
        "embedded_key_text.json",
        "{\n"
        "  \"note\": \"example \\\"enable_full_width_alnum\\\": true\",\n"
        "  \"enable_full_width_alnum\": false,\n"
        "  \"focus_loss_behavior\": \"KEEP\"\n"
        "}\n");

    const auto config = TextServiceConfig::loadFromFile(path);

    EXPECT_FALSE(config.enableFullWidthAlnum);
    EXPECT_EQ(config.focusLossBehavior, TextServiceConfig::FocusLossBehavior::Keep);
}

TEST(TextServiceConfigTest, StringSettersNormalizeIntoEnums) {
    TextServiceConfig config;
    config.setCapsLockBehavior("IME_TOGGLE");
    config.setFocusLossBehavior("KEEP");
    config.setEscapeInConversion("COMMIT");
    config.setBackspaceInConversion("unexpected");

    EXPECT_EQ(config.capsLockBehavior, TextServiceConfig::CapsLockBehavior::ImeToggle);
    EXPECT_EQ(config.focusLossBehavior, TextServiceConfig::FocusLossBehavior::Keep);
    EXPECT_EQ(config.escapeInConversion, TextServiceConfig::ConversionCancelBehavior::Commit);
    EXPECT_EQ(
        config.backspaceInConversion,
        TextServiceConfig::ConversionCancelBehavior::CancelToComposing);
}
