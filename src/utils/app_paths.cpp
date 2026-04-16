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


#include "utils/app_paths.hpp"

#include <windows.h>

#include <optional>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace yume::utils::paths {

namespace {

std::filesystem::path fallbackAbsoluteDirectory() {
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        const DWORD copied = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
        if (copied == 0) {
            break;
        }
        if (copied < buffer.size()) {
            return std::filesystem::path(std::wstring(buffer.data(), copied)).lexically_normal();
        }
        buffer.resize(copied + 1);
    }

    std::error_code ec;
    const auto current = std::filesystem::current_path(ec);
    if (!ec && current.is_absolute()) {
        return current;
    }

    return std::filesystem::path(L"C:\\");
}

std::filesystem::path currentModulePath() {
    HMODULE moduleHandle = reinterpret_cast<HMODULE>(&__ImageBase);

    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        const DWORD copied = GetModuleFileNameW(moduleHandle, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0) {
            return fallbackAbsoluteDirectory() / "yume-ime.exe";
        }
        if (copied < buffer.size() - 1) {
            return std::filesystem::path(std::wstring(buffer.data(), copied));
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::optional<std::filesystem::path> pathFromUtf8(std::string_view text) {
    std::filesystem::path path;
    size_t start = 0;
    while (start < text.size()) {
        const size_t slash = text.find('/', start);
        const auto part = text.substr(start, slash == std::string_view::npos ? text.size() - start : slash - start);
        if (!part.empty()) {
            if (part == "." || part == ".." || part.find('\\') != std::string_view::npos ||
                part.find(':') != std::string_view::npos) {
                return std::nullopt;
            }
            path /= std::filesystem::path(std::string(part));
        }
        if (slash == std::string_view::npos) {
            break;
        }
        start = slash + 1;
    }
    if (path.is_absolute()) {
        return std::nullopt;
    }
    return path;
}

}

std::filesystem::path moduleDirectory() {
    return currentModulePath().parent_path();
}

std::filesystem::path dataDirectory() {
    return moduleDirectory() / "data";
}

std::filesystem::path dataPath(std::string_view relativePath) {
    const auto relative = pathFromUtf8(relativePath);
    if (!relative.has_value()) {
        return dataDirectory();
    }
    return dataDirectory() / *relative;
}

}
