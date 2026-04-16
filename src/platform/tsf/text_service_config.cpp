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



#include "platform/tsf/text_service_config.hpp"

#include "utils/app_paths.hpp"
#include "utils/logger.hpp"

#include <cctype>
#include <fstream>
#include <utility>

namespace yume::platform::tsf {

namespace {

bool isHexDigit(char ch) {
    return std::isdigit(static_cast<unsigned char>(ch)) != 0 ||
           (ch >= 'a' && ch <= 'f') ||
           (ch >= 'A' && ch <= 'F');
}

uint32_t hexDigitValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return static_cast<uint32_t>(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f') {
        return static_cast<uint32_t>(ch - 'a' + 10);
    }
    return static_cast<uint32_t>(ch - 'A' + 10);
}

bool parseUnicodeEscape(
    const std::string& json,
    size_t escapeIndex,
    size_t& consumedChars,
    uint32_t& codePoint) {
    if (escapeIndex + 4 >= json.size()) {
        return false;
    }

    uint32_t firstUnit = 0;
    for (size_t hexIndex = escapeIndex + 1; hexIndex <= escapeIndex + 4; ++hexIndex) {
        if (!isHexDigit(json[hexIndex])) {
            return false;
        }
        firstUnit = (firstUnit << 4) | hexDigitValue(json[hexIndex]);
    }

    consumedChars = 4;
    codePoint = firstUnit;

    if (firstUnit >= 0xD800 && firstUnit <= 0xDBFF) {
        if (escapeIndex + 10 >= json.size() ||
            json[escapeIndex + 5] != '\\' ||
            json[escapeIndex + 6] != 'u') {
            return false;
        }

        uint32_t secondUnit = 0;
        for (size_t hexIndex = escapeIndex + 7; hexIndex <= escapeIndex + 10; ++hexIndex) {
            if (!isHexDigit(json[hexIndex])) {
                return false;
            }
            secondUnit = (secondUnit << 4) | hexDigitValue(json[hexIndex]);
        }

        if (secondUnit < 0xDC00 || secondUnit > 0xDFFF) {
            return false;
        }

        codePoint = 0x10000 + (((firstUnit - 0xD800) << 10) | (secondUnit - 0xDC00));
        consumedChars = 10;
        return true;
    }

    if (firstUnit >= 0xDC00 && firstUnit <= 0xDFFF) {
        return false;
    }

    return true;
}

void appendUtf8(std::string& outValue, uint32_t codePoint) {
    if (codePoint <= 0x7F) {
        outValue.push_back(static_cast<char>(codePoint));
        return;
    }
    if (codePoint <= 0x7FF) {
        outValue.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        outValue.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return;
    }
    if (codePoint <= 0xFFFF) {
        outValue.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        outValue.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        outValue.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        return;
    }

    outValue.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
    outValue.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
    outValue.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    outValue.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
}

}
std::filesystem::path TextServiceConfig::defaultConfigPath() {
    return yume::utils::paths::dataPath("settings/text_service_config.json");
}

TextServiceConfig TextServiceConfig::loadFromFile(const std::filesystem::path& path) {
    TextServiceConfig config;

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return config;
    }

    std::string json(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    if (json.empty()) {
        return config;
    }

    loadStringValue(
        json,
        "compatibility_style",
        [&config](std::string value) { config.setCompatibilityStyle(std::move(value)); });
    loadStringValue(
        json,
        "caps_lock_behavior",
        [&config](std::string value) { config.setCapsLockBehavior(std::move(value)); });
    {
        std::string parsedPrintableDuringConversion;
        const ParseStatus status = findStringValue(
            json,
            "printable_during_conversion",
            parsedPrintableDuringConversion);
        if (status == ParseStatus::Parsed) {
            if (isKnownPrintableDuringConversionValue(parsedPrintableDuringConversion)) {
                config.setPrintableDuringConversion(std::move(parsedPrintableDuringConversion));
            } else {
                YUME_LOG_WARN(
                    "TextServiceConfig",
                    "invalid string value for key=printable_during_conversion, using default");
            }
        } else if (status == ParseStatus::Invalid) {
            YUME_LOG_WARN(
                "TextServiceConfig",
                "invalid string value for key=printable_during_conversion, using default");
        }
    }
    loadStringValue(
        json,
        "focus_loss_behavior",
        [&config](std::string value) { config.setFocusLossBehavior(std::move(value)); });
    loadStringValue(
        json,
        "escape_in_conversion",
        [&config](std::string value) { config.setEscapeInConversion(std::move(value)); });
    loadStringValue(
        json,
        "backspace_in_conversion",
        [&config](std::string value) { config.setBackspaceInConversion(std::move(value)); });
    loadBoolValue(json, "enable_katakana", config.enableKatakana);
    loadBoolValue(json, "enable_half_katakana", config.enableHalfKatakana);
    loadBoolValue(json, "enable_full_width_alnum", config.enableFullWidthAlnum);
    config.normalize();
    return config;
}

template <typename Setter>
void TextServiceConfig::loadStringValue(const std::string& json, const char* key, Setter&& setter) {
    std::string parsed;
    const ParseStatus status = findStringValue(json, key, parsed);
    if (status == ParseStatus::Parsed) {
        setter(std::move(parsed));
        return;
    }
    if (status == ParseStatus::Invalid) {
        YUME_LOG_WARN("TextServiceConfig", "invalid string value for key=", key, ", using default");
    }
}

void TextServiceConfig::loadBoolValue(const std::string& json, const char* key, bool& outValue) {
    bool parsed = false;
    const ParseStatus status = findBoolValue(json, key, parsed);
    if (status == ParseStatus::Parsed) {
        outValue = parsed;
        return;
    }
    if (status == ParseStatus::Invalid) {
        YUME_LOG_WARN("TextServiceConfig", "invalid bool value for key=", key, ", using default");
    }
}

TextServiceConfig::CompatibilityStyle TextServiceConfig::parseCompatibilityStyle(std::string value) {
    normalizeLowerAscii(value);
    if (value == kDefaultCompatibilityStyle) {
        return CompatibilityStyle::ModernJp;
    }
    return CompatibilityStyle::ModernJp;
}

TextServiceConfig::CapsLockBehavior TextServiceConfig::parseCapsLockBehavior(std::string value) {
    normalizeLowerAscii(value);
    if (value == "disabled") {
        return CapsLockBehavior::Disabled;
    }
    if (value == "ime_toggle") {
        return CapsLockBehavior::ImeToggle;
    }
    return CapsLockBehavior::System;
}

bool TextServiceConfig::isKnownPrintableDuringConversionValue(std::string value) {
    normalizeLowerAscii(value);
    return value == kDefaultPrintableDuringConversion;
}

TextServiceConfig::PrintableDuringConversionBehavior TextServiceConfig::parsePrintableDuringConversion(
    std::string value) {
    normalizeLowerAscii(value);
    return PrintableDuringConversionBehavior::CommitAndInsert;
}

TextServiceConfig::FocusLossBehavior TextServiceConfig::parseFocusLossBehavior(std::string value) {
    normalizeLowerAscii(value);
    if (value == "keep") {
        return FocusLossBehavior::Keep;
    }
    return FocusLossBehavior::Commit;
}

TextServiceConfig::ConversionCancelBehavior TextServiceConfig::parseConversionCancelBehavior(std::string value) {
    normalizeLowerAscii(value);
    if (value == kCommitConversionBehavior) {
        return ConversionCancelBehavior::Commit;
    }
    return ConversionCancelBehavior::CancelToComposing;
}

std::string_view TextServiceConfig::conversionCancelBehaviorName(ConversionCancelBehavior behavior) {
    switch (behavior) {
        case ConversionCancelBehavior::CancelToComposing:
            return kDefaultConversionCancelBehavior;
        case ConversionCancelBehavior::Commit:
            return kCommitConversionBehavior;
    }
    return kDefaultConversionCancelBehavior;
}

void TextServiceConfig::normalizeLowerAscii(std::string& value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
}

bool TextServiceConfig::isEscaped(const std::string& json, size_t pos) {
    size_t backslashCount = 0;
    while (pos > 0 && json[--pos] == '\\') {
        ++backslashCount;
    }
    return (backslashCount % 2) != 0;
}

std::optional<size_t> TextServiceConfig::findKeyToken(const std::string& json, const char* key) {
    const std::string pattern = std::string("\"") + key + "\"";
    bool inString = false;

    for (size_t i = 0; i < json.size(); ++i) {
        if (json[i] != '"' || isEscaped(json, i)) {
            continue;
        }

        inString = !inString;
        if (inString && json.compare(i, pattern.size(), pattern) == 0) {
            const size_t afterToken = i + pattern.size();
            size_t cursor = afterToken;
            while (cursor < json.size() &&
                   (json[cursor] == ' ' || json[cursor] == '\t' || json[cursor] == '\r' || json[cursor] == '\n')) {
                ++cursor;
            }
            if (cursor < json.size() && json[cursor] == ':') {
                return i;
            }
        }
    }

    return std::nullopt;
}

TextServiceConfig::ParseStatus TextServiceConfig::findBoolValue(
    const std::string& json,
    const char* key,
    bool& outValue) {
    size_t valuePos = 0;
    const ParseStatus startStatus = findValueStart(json, key, valuePos);
    if (startStatus != ParseStatus::Parsed) {
        return startStatus;
    }

    if (json.compare(valuePos, 4, "true") == 0 && isJsonTerminal(json, valuePos + 4)) {
        outValue = true;
        return ParseStatus::Parsed;
    }
    if (json.compare(valuePos, 5, "false") == 0 && isJsonTerminal(json, valuePos + 5)) {
        outValue = false;
        return ParseStatus::Parsed;
    }
    return ParseStatus::Invalid;
}

TextServiceConfig::ParseStatus TextServiceConfig::findStringValue(
    const std::string& json,
    const char* key,
    std::string& outValue) {
    size_t valuePos = 0;
    const ParseStatus startStatus = findValueStart(json, key, valuePos);
    if (startStatus != ParseStatus::Parsed) {
        return startStatus;
    }

    if (valuePos >= json.size() || json[valuePos] != '"') {
        return ParseStatus::Invalid;
    }

    outValue.clear();
    outValue.reserve(32);
    for (size_t i = valuePos + 1; i < json.size(); ++i) {
        const unsigned char raw = static_cast<unsigned char>(json[i]);
        const char ch = static_cast<char>(raw);
        if (raw < 0x20) {
            return ParseStatus::Invalid;
        }
        if (ch == '"') {
            return ParseStatus::Parsed;
        }
        if (ch == '\\') {
            ++i;
            if (i >= json.size()) {
                return ParseStatus::Invalid;
            }
            const char escaped = json[i];
            switch (escaped) {
                case '"': outValue.push_back('"'); break;
                case '\\': outValue.push_back('\\'); break;
                case '/': outValue.push_back('/'); break;
                case 'b': outValue.push_back('\b'); break;
                case 'f': outValue.push_back('\f'); break;
                case 'n': outValue.push_back('\n'); break;
                case 'r': outValue.push_back('\r'); break;
                case 't': outValue.push_back('\t'); break;
                case 'u': {
                    size_t consumedChars = 0;
                    uint32_t codePoint = 0;
                    if (!parseUnicodeEscape(json, i, consumedChars, codePoint)) {
                        return ParseStatus::Invalid;
                    }
                    appendUtf8(outValue, codePoint);
                    i += consumedChars;
                    break;
                }
                default: return ParseStatus::Invalid;
            }
            continue;
        }
        outValue.push_back(ch);
    }
    return ParseStatus::Invalid;
}

TextServiceConfig::ParseStatus TextServiceConfig::findValueStart(
    const std::string& json,
    const char* key,
    size_t& outValuePos) {
    const auto keyPos = findKeyToken(json, key);
    if (!keyPos.has_value()) {
        return ParseStatus::Missing;
    }

    const size_t colonPos = json.find(':', *keyPos);
    if (colonPos == std::string::npos) {
        return ParseStatus::Invalid;
    }

    outValuePos = json.find_first_not_of(" \t\r\n", colonPos + 1);
    if (outValuePos == std::string::npos) {
        return ParseStatus::Invalid;
    }
    return ParseStatus::Parsed;
}

bool TextServiceConfig::isJsonTerminal(const std::string& json, size_t pos) {
    if (pos >= json.size()) {
        return true;
    }
    const char ch = json[pos];
    return ch == ',' || ch == '}' || ch == ']' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

}
