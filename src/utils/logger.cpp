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



#include "utils/logger.hpp"
#include "utils/app_paths.hpp"

#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <windows.h>

namespace yume::utils {

namespace {

constexpr uintmax_t kMaxLogSizeBytes = 10 * 1024 * 1024;
constexpr int kMaxLogBackups = 5;

std::filesystem::path logDirectory() {
    return yume::utils::paths::dataPath("logs");
}

std::filesystem::path logConfigPath() {
    return yume::utils::paths::dataPath("settings/logging_config.json");
}

std::string timestampNow() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto time = clock::to_time_t(now);

    std::tm localTime{};
    localtime_s(&localTime, &time);

    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
           << '.' << std::setw(3) << std::setfill('0') << millis.count();
    return stream.str();
}

bool isEscaped(const std::string& json, size_t pos) {
    size_t backslashCount = 0;
    while (pos > 0 && json[--pos] == '\\') {
        ++backslashCount;
    }
    return (backslashCount % 2) != 0;
}

std::optional<size_t> findKeyToken(const std::string& json, std::string_view key) {
    const std::string pattern = "\"" + std::string(key) + "\"";
    bool inString = false;

    for (size_t i = 0; i < json.size(); ++i) {
        if (json[i] != '"' || isEscaped(json, i)) {
            continue;
        }

        inString = !inString;
        if (inString && json.compare(i, pattern.size(), pattern) == 0) {
            size_t cursor = i + pattern.size();
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

bool findJsonStringValue(const std::string& json, std::string_view key, std::string& outValue) {
    const auto keyPos = findKeyToken(json, key);
    if (!keyPos.has_value()) {
        return false;
    }

    const size_t colonPos = json.find(':', *keyPos);
    if (colonPos == std::string::npos) {
        return false;
    }

    const size_t valuePos = json.find_first_not_of(" \t\r\n", colonPos + 1);
    if (valuePos == std::string::npos || json[valuePos] != '"') {
        return false;
    }

    outValue.clear();
    for (size_t i = valuePos + 1; i < json.size(); ++i) {
        const unsigned char raw = static_cast<unsigned char>(json[i]);
        const char ch = static_cast<char>(raw);
        if (raw < 0x20) {
            return false;
        }
        if (ch == '"' && !isEscaped(json, i)) {
            return true;
        }
        if (ch == '\\') {
            ++i;
            if (i >= json.size()) {
                return false;
            }
            switch (json[i]) {
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
                        return false;
                    }
                    appendUtf8(outValue, codePoint);
                    i += consumedChars;
                    break;
                }
                default: return false;
            }
            continue;
        }
        outValue.push_back(ch);
    }

    return false;
}

}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::log(Level level, std::string_view component, std::string_view message) {
    std::scoped_lock lock(mutex);
    if (!ensureReady()) {
        return;
    }
    if (level < minLevel) {
        return;
    }
    rotateIfNeeded();

    std::ofstream output(logPath, std::ios::app | std::ios::binary);
    if (!output.is_open()) {
        return;
    }

    output << timestampNow()
           << " [tid=" << GetCurrentThreadId() << "] "
           << '[' << levelText(level) << "] "
           << '[' << component << "] "
           << message << '\n';
}

void Logger::log_hresult(Level level, std::string_view component, std::string_view operation, long hr) {
    std::ostringstream stream;
    stream << operation << " failed hr=0x"
           << std::uppercase << std::hex << static_cast<unsigned long>(hr)
           << std::dec << " (" << hr << ')';
    log(level, component, stream.str());
}

bool Logger::ensureReady() {
    if (initialized) {
        return !logPath.empty();
    }

    const auto directory = logDirectory();
    std::error_code ec;
    const auto resolvedLogPath = logPath.empty()
        ? (directory / "yume-ime.log")
        : logPath;
    std::filesystem::create_directories(resolvedLogPath.parent_path(), ec);
    if (ec) {
        logPath.clear();
        minLevel = Level::Warn;
        return false;
    }
    logPath = resolvedLogPath;
    loadConfig();
    initialized = true;
    return true;
}

void Logger::rotateIfNeeded() {
    std::error_code ec;
    if (!std::filesystem::exists(logPath, ec)) {
        return;
    }
    if (std::filesystem::file_size(logPath, ec) < kMaxLogSizeBytes) {
        return;
    }

    for (int i = kMaxLogBackups; i >= 1; --i) {
        const auto source = (i == 1)
            ? logPath
            : logPath.parent_path() / ("yume-ime.log." + std::to_string(i - 1));
        const auto target = logPath.parent_path() / ("yume-ime.log." + std::to_string(i));
        std::filesystem::remove(target, ec);
        if (std::filesystem::exists(source, ec)) {
            std::filesystem::rename(source, target, ec);
        }
    }
}

const char* Logger::levelText(Level level) {
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
        default: return "UNKNOWN";
    }
}

Logger::Level Logger::minimumLevel() const {
    return minLevel;
}

void Logger::resetForTesting() {
    std::scoped_lock lock(mutex);
    initialized = false;
    logPath.clear();
    configPathOverride.clear();
    minLevel = Level::Warn;
}

void Logger::setPathsForTesting(std::filesystem::path logFilePath, std::filesystem::path configFilePath) {
    std::scoped_lock lock(mutex);
    initialized = false;
    logPath = std::move(logFilePath);
    configPathOverride = std::move(configFilePath);
    minLevel = Level::Warn;
}

void Logger::loadConfig() {
    minLevel = Level::Warn;

    const auto configPath = configPathOverride.empty() ? logConfigPath() : configPathOverride;
    std::ifstream input(configPath, std::ios::binary);
    if (!input.is_open()) {
        return;
    }

    const std::string json(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    std::string value;
    if (!findJsonStringValue(json, "minimum_level", value)) {
        return;
    }

    minLevel = parseLevel(value);
}

Logger::Level Logger::parseLevel(std::string_view value) {
    std::string normalized(value);
    for (char& ch : normalized) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }

    if (normalized == "trace") {
        return Level::Trace;
    }
    if (normalized == "debug") {
        return Level::Debug;
    }
    if (normalized == "info") {
        return Level::Info;
    }
    if (normalized == "warn" || normalized == "warning") {
        return Level::Warn;
    }
    if (normalized == "error") {
        return Level::Error;
    }

    return Level::Warn;
}

}
