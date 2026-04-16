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

#include <utility>

#include <unknwn.h>
#include <windows.h>

namespace yume::utils {

    template <typename Derived>
    class ComObjectBase {
    public:
        ULONG STDMETHODCALLTYPE AddRef() {
            return static_cast<ULONG>(::InterlockedIncrement(&referenceCount_));
        }

        ULONG STDMETHODCALLTYPE Release() {
            const ULONG count = static_cast<ULONG>(::InterlockedDecrement(&referenceCount_));
            if (count == 0) {
                delete static_cast<Derived*>(this);
            }
            return count;
        }

        static HRESULT queryInterfacePointer(void** output, auto* value) {
            if (output == nullptr) {
                return E_POINTER;
            }

            *output = value;
            if (value == nullptr) {
                return E_NOINTERFACE;
            }

            value->AddRef();
            return S_OK;
        }

        static HRESULT queryInterfacePointer(void** output, std::nullptr_t) {
            if (output == nullptr) {
                return E_POINTER;
            }

            *output = nullptr;
            return E_NOINTERFACE;
        }

    protected:
        ComObjectBase() = default;
        ~ComObjectBase() = default;

    private:
        volatile long referenceCount_ = 1;
    };

}
