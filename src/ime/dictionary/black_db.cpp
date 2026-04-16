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



#include "ime/dictionary/black_db.hpp"

#include "ime/dictionary/db_binary_io.hpp"
#include "ime/dictionary/db_types.hpp"
#include "utils/logger.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace yume::ime::dictionary {

namespace {

constexpr uint32_t kBlackDbMagic = 0x42444231; // BlackDb file signature.
}
bool BlackDb::isBlocked(ReadingView reading, SurfaceView text) const {
    return blockedEntries.find(makeEntryKey(reading, text)) != blockedEntries.end();
}

void BlackDb::block(ReadingView reading, SurfaceView text) {
    if (reading.empty() || text.empty()) {
        return;
    }
    if (blockedEntries.emplace(makeEntryKey(reading, text)).second) {
        ++dbGeneration;
    }
}

bool BlackDb::loadFromFile(const std::filesystem::path& path) {
    *this = BlackDb{};
    BlackDb loaded;

    const binary_io::PathState state = binary_io::pathStateNoThrow(path, "BlackDb");
    if (state == binary_io::PathState::Missing) {
        *this = std::move(loaded);
        YUME_LOG_INFO("BlackDb", "load skipped missing path=", path.string());
        return true;
    }
    if (state == binary_io::PathState::Error) {
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        YUME_LOG_ERROR("BlackDb", "failed to open path=", path.string());
        return false;
    }

    uint32_t count = 0;
    if (!binary_io::readEntryCount(input, kBlackDbMagic, count)) {
        YUME_LOG_ERROR("BlackDb", "invalid header path=", path.string(), " count=", count);
        return false;
    }

    for (uint32_t i = 0; i < count; ++i) {
        std::u16string reading;
        std::u16string text;
        if (!binary_io::readString(input, reading) || !binary_io::readString(input, text)) {
            return false;
        }
        loaded.blockedEntries.emplace(makeEntryKey(reading, text));
    }

    ++loaded.dbGeneration;
    *this = std::move(loaded);
    YUME_LOG_INFO("BlackDb", "loaded entries=", blockedEntries.size(), " path=", path.string());
    return true;
}

bool BlackDb::saveToFile(const std::filesystem::path& path) const {
    if (!binary_io::ensureParentDirectory(path)) {
        YUME_LOG_ERROR("BlackDb", "failed to create parent directory path=", path.string());
        return false;
    }

    if (blockedEntries.size() > binary_io::kMaxEntryCount) {
        YUME_LOG_ERROR("BlackDb", "too many entries to save path=", path.string(), " count=", blockedEntries.size());
        return false;
    }

    const std::filesystem::path tempPath = path.native() + L".tmp";
    std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        YUME_LOG_ERROR("BlackDb", "failed to open temp for save path=", tempPath.string());
        return false;
    }

    if (!binary_io::writeEntryCount(output, kBlackDbMagic, blockedEntries.size())) {
        output.close();
        binary_io::removeNoThrow(tempPath);
        return false;
    }

    for (const auto& key : blockedEntries) {
        const auto separator = key.find(u'\t');
        const auto reading = key.substr(0, separator);
        const auto text = separator == std::u16string::npos ? std::u16string{} : key.substr(separator + 1);
        if (!binary_io::writeString(output, reading) || !binary_io::writeString(output, text)) {
            output.close();
            binary_io::removeNoThrow(tempPath);
            return false;
        }
    }

    if (!binary_io::finalizeAtomicSave(output, tempPath, path, "BlackDb")) {
        return false;
    }
    YUME_LOG_INFO("BlackDb", "saved entries=", blockedEntries.size(), " path=", path.string());
    return true;
}

}
