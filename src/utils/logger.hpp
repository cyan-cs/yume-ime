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

#include <filesystem>
#include <mutex>
#include <sstream>
#include <string_view>

namespace yume::utils {

    class Logger final {
    public:
        enum class Level {
            Trace,
            Debug,
            Info,
            Warn,
            Error,
        };

        static Logger& instance();

        void log(Level level, std::string_view component, std::string_view message);
        void log_hresult(Level level, std::string_view component, std::string_view operation, long hr);
        Level minimumLevel() const;
        void resetForTesting();
        void setPathsForTesting(std::filesystem::path logFilePath, std::filesystem::path configFilePath);

    private:
        Logger() = default;

        bool ensureReady();
        void rotateIfNeeded();
        void loadConfig();
        static const char* levelText(Level level);
        static Level parseLevel(std::string_view value);

        std::mutex mutex;
        std::filesystem::path logPath;
        std::filesystem::path configPathOverride;
        bool initialized = false;
        Level minLevel = Level::Warn;
    };

    template <typename... Args>
    inline void logf(Logger::Level level, std::string_view component, Args&&... args) {
        std::ostringstream stream;
        (stream << ... << std::forward<Args>(args));
        Logger::instance().log(level, component, stream.str());
    }

}

#define YUME_LOG_TRACE(component, ...) ::yume::utils::logf(::yume::utils::Logger::Level::Trace, component, __VA_ARGS__)
#define YUME_LOG_DEBUG(component, ...) ::yume::utils::logf(::yume::utils::Logger::Level::Debug, component, __VA_ARGS__)
#define YUME_LOG_INFO(component, ...) ::yume::utils::logf(::yume::utils::Logger::Level::Info, component, __VA_ARGS__)
#define YUME_LOG_WARN(component, ...) ::yume::utils::logf(::yume::utils::Logger::Level::Warn, component, __VA_ARGS__)
#define YUME_LOG_ERROR(component, ...) ::yume::utils::logf(::yume::utils::Logger::Level::Error, component, __VA_ARGS__)
#define YUME_LOG_HRESULT(level, component, operation, hr) \
    ::yume::utils::Logger::instance().log_hresult(level, component, operation, static_cast<long>(hr))
