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

#include <windows.h>

#include <utility>

namespace yume::utils {

template <typename Handle, Handle InvalidValue, typename Traits>
class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(Handle handle) : handle_(handle) {}

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}

    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    ~UniqueHandle() {
        reset();
    }

    Handle get() const noexcept {
        return handle_;
    }

    explicit operator bool() const noexcept {
        return handle_ != InvalidValue;
    }

    Handle release() noexcept {
        const Handle handle = handle_;
        handle_ = InvalidValue;
        return handle;
    }

    void reset(Handle handle = InvalidValue) noexcept {
        if (handle_ != InvalidValue) {
            Traits::close(handle_);
        }
        handle_ = handle;
    }

private:
    Handle handle_ = InvalidValue;
};

struct KernelHandleTraits {
    static void close(HANDLE handle) noexcept {
        ::CloseHandle(handle);
    }
};

struct WindowHandleTraits {
    static void close(HWND window) noexcept {
        ::DestroyWindow(window);
    }
};

struct RegistryKeyTraits {
    static void close(HKEY key) noexcept {
        ::RegCloseKey(key);
    }
};

struct GdiObjectTraits {
    static void close(HGDIOBJ object) noexcept {
        ::DeleteObject(object);
    }
};

using UniqueKernelHandle = UniqueHandle<HANDLE, nullptr, KernelHandleTraits>;
using UniqueWindowHandle = UniqueHandle<HWND, nullptr, WindowHandleTraits>;
using UniqueRegistryKey = UniqueHandle<HKEY, nullptr, RegistryKeyTraits>;
using UniqueGdiObject = UniqueHandle<HGDIOBJ, nullptr, GdiObjectTraits>;

class ScopedSelectedObject {
public:
    ScopedSelectedObject(HDC deviceContext, HGDIOBJ object) noexcept
        : deviceContext_(deviceContext),
          previous_(object != nullptr ? ::SelectObject(deviceContext, object) : nullptr) {}

    ScopedSelectedObject(const ScopedSelectedObject&) = delete;
    ScopedSelectedObject& operator=(const ScopedSelectedObject&) = delete;

    ~ScopedSelectedObject() {
        if (deviceContext_ != nullptr && previous_ != nullptr) {
            ::SelectObject(deviceContext_, previous_);
        }
    }

private:
    HDC deviceContext_ = nullptr;
    HGDIOBJ previous_ = nullptr;
};

class ScopedWindowDc {
public:
    explicit ScopedWindowDc(HWND window) noexcept : window_(window), deviceContext_(::GetDC(window)) {}

    ScopedWindowDc(const ScopedWindowDc&) = delete;
    ScopedWindowDc& operator=(const ScopedWindowDc&) = delete;

    ~ScopedWindowDc() {
        if (window_ != nullptr && deviceContext_ != nullptr) {
            ::ReleaseDC(window_, deviceContext_);
        }
    }

    HDC get() const noexcept {
        return deviceContext_;
    }

private:
    HWND window_ = nullptr;
    HDC deviceContext_ = nullptr;
};

class ScopedPaint {
public:
    explicit ScopedPaint(HWND window) noexcept : window_(window) {
        deviceContext_ = ::BeginPaint(window_, &paintStruct_);
    }

    ScopedPaint(const ScopedPaint&) = delete;
    ScopedPaint& operator=(const ScopedPaint&) = delete;

    ~ScopedPaint() {
        if (window_ != nullptr && deviceContext_ != nullptr) {
            ::EndPaint(window_, &paintStruct_);
        }
    }

    HDC get() const noexcept {
        return deviceContext_;
    }

private:
    HWND window_ = nullptr;
    PAINTSTRUCT paintStruct_{};
    HDC deviceContext_ = nullptr;
};

class ScopedComApartment {
public:
    explicit ScopedComApartment(DWORD coInit = COINIT_APARTMENTTHREADED) noexcept
        : initHr_(::CoInitializeEx(nullptr, coInit)) {}

    ScopedComApartment(const ScopedComApartment&) = delete;
    ScopedComApartment& operator=(const ScopedComApartment&) = delete;

    ~ScopedComApartment() {
        if (SUCCEEDED(initHr_)) {
            ::CoUninitialize();
        }
    }

    HRESULT status() const noexcept {
        return initHr_;
    }

private:
    HRESULT initHr_ = E_FAIL;
};

}
