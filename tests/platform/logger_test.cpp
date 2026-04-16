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

#include "utils/logger.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "utils/app_paths.hpp"

using yume::utils::Logger;

namespace {

std::filesystem::path loggingConfigPath() {
    return std::filesystem::path("build") / "gtest_logger" / "logging_config.json";
}

std::filesystem::path logFilePath() {
    return std::filesystem::path("build") / "gtest_logger" / "logs" / "yume-ime.log";
}

std::string readFileOrEmpty(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

void writeTextFile(const std::filesystem::path& path, const std::string& text) {
    std::error_code ec;
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            throw std::runtime_error("failed to prepare logger test directory");
        }
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        throw std::runtime_error("failed to open logger test file");
    }
    output << text;
    if (!output.good()) {
        throw std::runtime_error("failed to write logger test file");
    }
}

class ScopedFileRestore {
public:
    explicit ScopedFileRestore(std::filesystem::path path)
        : path_(std::move(path)),
          hadOriginal_(std::filesystem::exists(path_)),
          original_(readFileOrEmpty(path_)) {}

    ~ScopedFileRestore() noexcept {
        std::error_code ec;
        try {
            if (hadOriginal_) {
                writeTextFile(path_, original_);
            } else {
                std::filesystem::remove(path_, ec);
            }
        } catch (...) {
        }
    }

private:
    std::filesystem::path path_;
    bool hadOriginal_ = false;
    std::string original_;
};

}
TEST(LoggerTest, ConfigIgnoresEmbeddedMinimumLevelLikeTextInsideStrings) {
    const auto configPath = loggingConfigPath();
    const auto logPath = logFilePath();
    ScopedFileRestore restoreConfig(configPath);
    ScopedFileRestore restoreLog(logPath);
    std::error_code ec;
    std::filesystem::remove(logPath, ec);

    writeTextFile(
        configPath,
        "{\n"
        "  \"note\": \"fake \\\"minimum_level\\\": \\\"error\\\"\",\n"
        "  \"minimum_level\": \"warn\"\n"
        "}\n");

    Logger::instance().resetForTesting();
    Logger::instance().setPathsForTesting(logPath, configPath);
    Logger::instance().log(Logger::Level::Info, "LoggerTest", "info should be filtered");
    Logger::instance().log(Logger::Level::Warn, "LoggerTest", "warn should be written");

    const std::string content = readFileOrEmpty(logPath);
    EXPECT_EQ(Logger::instance().minimumLevel(), Logger::Level::Warn);
    EXPECT_EQ(content.find("info should be filtered"), std::string::npos);
    EXPECT_NE(content.find("warn should be written"), std::string::npos);
    Logger::instance().resetForTesting();
}

TEST(LoggerTest, ConfigRejectsUnicodeEscapeInMinimumLevel) {
    const auto configPath = loggingConfigPath();
    const auto logPath = logFilePath();
    ScopedFileRestore restoreConfig(configPath);
    ScopedFileRestore restoreLog(logPath);
    std::error_code ec;
    std::filesystem::remove(logPath, ec);

    writeTextFile(
        configPath,
        "{\n"
        "  \"minimum_level\": \"w\\u0061rn\"\n"
        "}\n");

    Logger::instance().resetForTesting();
    Logger::instance().setPathsForTesting(logPath, configPath);
    Logger::instance().log(Logger::Level::Info, "LoggerTest", "info should be written");
    Logger::instance().resetForTesting();
}
