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

#include <cstddef>

#include <msctf.h>
#include <wrl/client.h>

namespace yume::platform::tsf::helpers {

    template <typename TInterface, typename TSource>
    HRESULT queryInterface(TSource* source, Microsoft::WRL::ComPtr<TInterface>& result) {
        result.Reset();
        if (source == nullptr) {
            return E_POINTER;
        }

        return source->QueryInterface(
            __uuidof(TInterface),
            reinterpret_cast<void**>(result.GetAddressOf()));
    }

    inline HRESULT getActiveView(ITfContext* context, Microsoft::WRL::ComPtr<ITfContextView>& view) {
        view.Reset();
        if (context == nullptr) {
            return E_POINTER;
        }

        return context->GetActiveView(view.GetAddressOf());
    }

    inline HWND getActiveViewWindow(ITfContext* context) {
        Microsoft::WRL::ComPtr<ITfContextView> view;
        if (FAILED(getActiveView(context, view)) || view == nullptr) {
            return nullptr;
        }

        HWND window = nullptr;
        return SUCCEEDED(view->GetWnd(&window)) ? window : nullptr;
    }

    template <typename TSink>
    HRESULT adviseSink(
        ITfSource* source,
        REFIID sinkId,
        TSink* sink,
        DWORD* cookie) {
        if (source == nullptr || sink == nullptr || cookie == nullptr) {
            return E_POINTER;
        }

        *cookie = TF_INVALID_COOKIE;
        return source->AdviseSink(sinkId, sink, cookie);
    }

    inline void unadviseSink(ITfSource* source, DWORD* cookie) {
        if (source == nullptr || cookie == nullptr || *cookie == TF_INVALID_COOKIE) {
            return;
        }

        source->UnadviseSink(*cookie);
        *cookie = TF_INVALID_COOKIE;
    }

    HRESULT setCollapsedSelectionAtOffset(
        TfEditCookie editCookie,
        ITfContext* context,
        ITfRange* baseRange,
        size_t offset,
        bool interim);

}
