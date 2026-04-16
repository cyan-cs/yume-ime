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



#include "platform/tsf/text_service.hpp"

namespace yume::platform::tsf {

HRESULT STDMETHODCALLTYPE TextService::QueryInterface(REFIID riid, void** ppvObject) {
    if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor) {
        return yume::utils::ComObjectBase<TextService>::queryInterfacePointer(
            ppvObject,
            static_cast<ITfTextInputProcessor*>(this));
    }
    if (riid == IID_ITfKeyEventSink) {
        return yume::utils::ComObjectBase<TextService>::queryInterfacePointer(
            ppvObject,
            static_cast<ITfKeyEventSink*>(this));
    }
    if (riid == IID_ITfThreadMgrEventSink) {
        return yume::utils::ComObjectBase<TextService>::queryInterfacePointer(
            ppvObject,
            static_cast<ITfThreadMgrEventSink*>(this));
    }
    if (riid == IID_ITfCompositionSink) {
        return yume::utils::ComObjectBase<TextService>::queryInterfacePointer(
            ppvObject,
            static_cast<ITfCompositionSink*>(this));
    }
    if (riid == IID_ITfUIElementSink) {
        return yume::utils::ComObjectBase<TextService>::queryInterfacePointer(
            ppvObject,
            static_cast<ITfUIElementSink*>(this));
    }

    return yume::utils::ComObjectBase<TextService>::queryInterfacePointer(ppvObject, nullptr);
}

ULONG STDMETHODCALLTYPE TextService::AddRef() {
    return yume::utils::ComObjectBase<TextService>::AddRef();
}

ULONG STDMETHODCALLTYPE TextService::Release() {
    return yume::utils::ComObjectBase<TextService>::Release();
}

}
