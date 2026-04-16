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

#include "utils/logger.hpp"

#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <windows.h>

namespace yume::ime::dictionary::binary_io {

enum class PathState {
    Missing,
    Exists,
    Error,
};

constexpr uint32_t kMaxStringLength = 65536;
constexpr uint32_t kMaxEntryCount = 100000;

inline bool readString(std::ifstream& input, std::u16string& value) {
    uint32_t length = 0;
    input.read(reinterpret_cast<char*>(&length), sizeof(length));
    if (!input.good() || length > kMaxStringLength) {
        return false;
    }

    value.resize(length);
    if (length == 0) {
        return true;
    }

    input.read(reinterpret_cast<char*>(value.data()), static_cast<std::streamsize>(length * sizeof(char16_t)));
    return input.good();
}

inline bool writeString(std::ofstream& output, std::u16string_view value) {
    if (value.size() > kMaxStringLength) {
        return false;
    }

    const auto length = static_cast<uint32_t>(value.size());
    output.write(reinterpret_cast<const char*>(&length), sizeof(length));
    if (!output.good()) {
        return false;
    }

    if (length == 0) {
        return true;
    }

    output.write(reinterpret_cast<const char*>(value.data()), static_cast<std::streamsize>(length * sizeof(char16_t)));
    return output.good();
}

inline bool writeEntryCount(std::ofstream& output, uint32_t magic, size_t count) {
    if (count > kMaxEntryCount) {
        return false;
    }

    const auto persistedCount = static_cast<uint32_t>(count);
    output.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    output.write(reinterpret_cast<const char*>(&persistedCount), sizeof(persistedCount));
    return output.good();
}

inline bool readEntryCount(std::ifstream& input, uint32_t expectedMagic, uint32_t& outCount) {
    uint32_t magic = 0;
    input.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    input.read(reinterpret_cast<char*>(&outCount), sizeof(outCount));
    return input.good() && magic == expectedMagic && outCount <= kMaxEntryCount;
}

inline bool replaceFileAtomically(const std::filesystem::path& from, const std::filesystem::path& to) {
    return MoveFileExW(
               from.c_str(),
               to.c_str(),
               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

inline void removeNoThrow(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

inline bool finalizeAtomicSave(
    std::ofstream& output,
    const std::filesystem::path& tempPath,
    const std::filesystem::path& path,
    std::string_view label) {
    output.flush();
    const bool ok = output.good();
    output.close();
    if (!ok) {
        removeNoThrow(tempPath);
        return false;
    }

    if (!replaceFileAtomically(tempPath, path)) {
        removeNoThrow(tempPath);
        YUME_LOG_ERROR(
            label,
            "failed to replace saved file path=",
            path.string(),
            " temp=",
            tempPath.string());
        return false;
    }

    return true;
}

inline bool ensureParentDirectory(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if (parent.empty()) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    return !ec;
}

inline PathState pathStateNoThrow(const std::filesystem::path& path, std::string_view label) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        YUME_LOG_ERROR(label, "exists check failed path=", path.string());
        return PathState::Error;
    }
    return exists ? PathState::Exists : PathState::Missing;
}

}
