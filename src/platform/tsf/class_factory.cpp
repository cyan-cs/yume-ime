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


#include "platform/tsf/class_factory.hpp"

#include "platform/tsf/module_state.hpp"
#include "platform/tsf/text_service.hpp"
#include "utils/com_ptr.hpp"

#include <wrl/client.h>

namespace yume::platform::tsf {

volatile long ClassFactory::globalServerLocks = 0;

ClassFactory::ClassFactory()
    : objectLease(ModuleState::instance().acquireObjectLease()) {}

HRESULT STDMETHODCALLTYPE ClassFactory::QueryInterface(REFIID riid, void** ppvObject) {
    if (riid == IID_IUnknown || riid == IID_IClassFactory) {
        return yume::utils::ComObjectBase<ClassFactory>::queryInterfacePointer(
            ppvObject,
            static_cast<IClassFactory*>(this));
    }

    return yume::utils::ComObjectBase<ClassFactory>::queryInterfacePointer(ppvObject, nullptr);
}

ULONG STDMETHODCALLTYPE ClassFactory::AddRef() {
    return yume::utils::ComObjectBase<ClassFactory>::AddRef();
}

ULONG STDMETHODCALLTYPE ClassFactory::Release() {
    return yume::utils::ComObjectBase<ClassFactory>::Release();
}

HRESULT STDMETHODCALLTYPE ClassFactory::CreateInstance(IUnknown* outer, REFIID riid, void** object) {
    if (object == nullptr) {
        return E_POINTER;
    }
    *object = nullptr;

    if (outer != nullptr) {
        return CLASS_E_NOAGGREGATION;
    }

    auto service = yume::utils::makeComPtr<TextService>();
    if (service == nullptr) {
        return E_OUTOFMEMORY;
    }

    return service->QueryInterface(riid, object);
}

HRESULT STDMETHODCALLTYPE ClassFactory::LockServer(BOOL lock) {
    if (lock) {
        InterlockedIncrement(&globalServerLocks);
    } else {
        InterlockedDecrement(&globalServerLocks);
    }
    return S_OK;
}

long ClassFactory::serverLockCount() {
    return globalServerLocks;
}

ClassFactory::~ClassFactory() = default;

}
