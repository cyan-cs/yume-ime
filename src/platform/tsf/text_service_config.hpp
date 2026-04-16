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

#include <optional>
#include <string>
#include <string_view>
#include <filesystem>

namespace yume::platform::tsf {

    struct TextServiceConfig {
        static constexpr const char* kDefaultCompatibilityStyle = "modern_jp";
        static constexpr const char* kDefaultCapsLockBehavior = "system";
        static constexpr const char* kDefaultPrintableDuringConversion = "commit_and_insert";
        static constexpr const char* kDefaultFocusLossBehavior = "commit";
        static constexpr const char* kDefaultConversionCancelBehavior = "cancel_to_composing";
        static constexpr const char* kCommitConversionBehavior = "commit";

        enum class CompatibilityStyle {
            ModernJp,
        };

        enum class CapsLockBehavior {
            System,
            ImeToggle,
            Disabled,
        };

        enum class PrintableDuringConversionBehavior {
            CommitAndInsert,
        };

        enum class FocusLossBehavior {
            Commit,
            Keep,
        };

        enum class ConversionCancelBehavior {
            CancelToComposing,
            Commit,
        };

        CompatibilityStyle compatibilityStyle = CompatibilityStyle::ModernJp;
        CapsLockBehavior capsLockBehavior = CapsLockBehavior::System;
        PrintableDuringConversionBehavior printableDuringConversion =
            PrintableDuringConversionBehavior::CommitAndInsert;
        FocusLossBehavior focusLossBehavior = FocusLossBehavior::Commit;
        ConversionCancelBehavior escapeInConversion = ConversionCancelBehavior::CancelToComposing;
        ConversionCancelBehavior backspaceInConversion = ConversionCancelBehavior::CancelToComposing;
        bool enableKatakana = false;
        bool enableHalfKatakana = false;
        bool enableFullWidthAlnum = false;

        void normalize() {
            if (enableHalfKatakana) {
                enableKatakana = true;
            }
        }

        void setCompatibilityStyle(std::string value) {
            compatibilityStyle = parseCompatibilityStyle(std::move(value));
        }

        void setCapsLockBehavior(std::string value) {
            capsLockBehavior = parseCapsLockBehavior(std::move(value));
        }

        void setPrintableDuringConversion(std::string value) {
            printableDuringConversion = parsePrintableDuringConversion(std::move(value));
        }

        void setFocusLossBehavior(std::string value) {
            focusLossBehavior = parseFocusLossBehavior(std::move(value));
        }

        void setEscapeInConversion(std::string value) {
            escapeInConversion = parseConversionCancelBehavior(std::move(value));
        }

        void setBackspaceInConversion(std::string value) {
            backspaceInConversion = parseConversionCancelBehavior(std::move(value));
        }

        bool usesImeToggleCapsLock() const {
            return capsLockBehavior == CapsLockBehavior::ImeToggle;
        }

        bool commitsPrintableDuringConversion() const {
            return printableDuringConversion == PrintableDuringConversionBehavior::CommitAndInsert;
        }

        bool commitsOnFocusLoss() const {
            return focusLossBehavior == FocusLossBehavior::Commit;
        }

        bool commitsEscapeDuringConversion() const {
            return escapeInConversion == ConversionCancelBehavior::Commit;
        }

        bool commitsBackspaceDuringConversion() const {
            return backspaceInConversion == ConversionCancelBehavior::Commit;
        }

        bool usesHalfKatakanaOutput() const {
            return enableHalfKatakana;
        }

        bool usesKatakanaOutput() const {
            return enableKatakana;
        }

        bool usesFullWidthDirectInput() const {
            return enableFullWidthAlnum;
        }

        std::string_view compatibilityStyleName() const {
            switch (compatibilityStyle) {
                case CompatibilityStyle::ModernJp:
                    return kDefaultCompatibilityStyle;
            }
            return kDefaultCompatibilityStyle;
        }

        std::string_view capsLockBehaviorName() const {
            switch (capsLockBehavior) {
                case CapsLockBehavior::System:
                    return kDefaultCapsLockBehavior;
                case CapsLockBehavior::ImeToggle:
                    return "ime_toggle";
                case CapsLockBehavior::Disabled:
                    return "disabled";
            }
            return kDefaultCapsLockBehavior;
        }

        std::string_view printableDuringConversionName() const {
            switch (printableDuringConversion) {
                case PrintableDuringConversionBehavior::CommitAndInsert:
                    return kDefaultPrintableDuringConversion;
            }
            return kDefaultPrintableDuringConversion;
        }

        std::string_view focusLossBehaviorName() const {
            switch (focusLossBehavior) {
                case FocusLossBehavior::Commit:
                    return kDefaultFocusLossBehavior;
                case FocusLossBehavior::Keep:
                    return "keep";
            }
            return kDefaultFocusLossBehavior;
        }

        std::string_view escapeInConversionName() const {
            return conversionCancelBehaviorName(escapeInConversion);
        }

        std::string_view backspaceInConversionName() const {
            return conversionCancelBehaviorName(backspaceInConversion);
        }

        static TextServiceConfig loadFromFile(
            const std::filesystem::path& path = defaultConfigPath());

    private:
        enum class ParseStatus {
            Missing,
            Parsed,
            Invalid,
        };

        static std::filesystem::path defaultConfigPath();

        template <typename Setter>
        static void loadStringValue(const std::string& json, const char* key, Setter&& setter);
        static void loadBoolValue(const std::string& json, const char* key, bool& outValue);
        static CompatibilityStyle parseCompatibilityStyle(std::string value);
        static CapsLockBehavior parseCapsLockBehavior(std::string value);
        static bool isKnownPrintableDuringConversionValue(std::string value);
        static PrintableDuringConversionBehavior parsePrintableDuringConversion(std::string value);
        static FocusLossBehavior parseFocusLossBehavior(std::string value);
        static ConversionCancelBehavior parseConversionCancelBehavior(std::string value);
        static std::string_view conversionCancelBehaviorName(ConversionCancelBehavior behavior);
        static void normalizeLowerAscii(std::string& value);
        static bool isEscaped(const std::string& json, size_t pos);
        static std::optional<size_t> findKeyToken(const std::string& json, const char* key);
        static ParseStatus findBoolValue(const std::string& json, const char* key, bool& outValue);
        static ParseStatus findStringValue(const std::string& json, const char* key, std::string& outValue);
        static ParseStatus findValueStart(const std::string& json, const char* key, size_t& outValuePos);
        static bool isJsonTerminal(const std::string& json, size_t pos);
    };

}
