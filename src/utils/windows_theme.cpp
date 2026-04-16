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



#include "utils/windows_theme.hpp"

#include "utils/win_raii.hpp"

#include <windows.h>

namespace yume::utils::windows_theme {

AppTheme appThemeFromAppsUseLightThemeValue(uint32_t value) {
    return value != 0 ? AppTheme::Light : AppTheme::Dark;
}

AppTheme queryAppTheme() {
    UniqueRegistryKey personalizeKey;
    HKEY rawKey = nullptr;
    const LSTATUS openStatus = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0,
        KEY_QUERY_VALUE,
        &rawKey);
    if (openStatus != ERROR_SUCCESS) {
        return AppTheme::Light;
    }
    personalizeKey.reset(rawKey);

    DWORD value = 1;
    DWORD valueType = 0;
    DWORD valueSize = sizeof(value);
    const LSTATUS queryStatus = RegQueryValueExW(
        personalizeKey.get(),
        L"AppsUseLightTheme",
        nullptr,
        &valueType,
        reinterpret_cast<LPBYTE>(&value),
        &valueSize);
    if (queryStatus != ERROR_SUCCESS || valueType != REG_DWORD || valueSize != sizeof(value)) {
        return AppTheme::Light;
    }

    return appThemeFromAppsUseLightThemeValue(value);
}

}
