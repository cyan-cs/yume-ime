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



#include "platform/tsf/text_service_ops.hpp"

#include "utils/com_ptr.hpp"
#include "utils/com_object_base.hpp"
#include "utils/logger.hpp"

namespace yume::platform::tsf {

using ime::engine::EngineOutput;

class TextEditSession final
    : public yume::utils::ComObjectBase<TextEditSession>
    , public ITfEditSession {
public:
    TextEditSession(TextService& owner, ITfContext* context, const EngineOutput& output)
        : owner(owner), context(context), output(output) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == IID_IUnknown || riid == IID_ITfEditSession) {
            return yume::utils::ComObjectBase<TextEditSession>::queryInterfacePointer(
                ppvObject,
                static_cast<ITfEditSession*>(this));
        }
        return yume::utils::ComObjectBase<TextEditSession>::queryInterfacePointer(ppvObject, nullptr);
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return yume::utils::ComObjectBase<TextEditSession>::AddRef();
    }

    ULONG STDMETHODCALLTYPE Release() override {
        return yume::utils::ComObjectBase<TextEditSession>::Release();
    }

    HRESULT STDMETHODCALLTYPE DoEditSession(TfEditCookie editCookie) override {
        return owner.applyEngineOutput(editCookie, context.Get(), output);
    }

private:
    friend class yume::utils::ComObjectBase<TextEditSession>;

    ~TextEditSession() = default;

    TextService& owner;
    Microsoft::WRL::ComPtr<ITfContext> context;
    EngineOutput output;
};

HRESULT TextService::EditOps::logEditSessionResult(const TextService& service, const char* action, HRESULT hr) {
    (void)service;
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", action, hr);
    }
    return hr;
}

HRESULT TextService::EditOps::requestReadWriteEditSession(
    const TextService& service,
    ITfContext* context,
    ITfEditSession* editSession,
    bool requireSynchronous) {
    if (context == nullptr || editSession == nullptr || service.clientId == TF_CLIENTID_NULL) {
        return E_UNEXPECTED;
    }

    HRESULT sessionHr = E_FAIL;
    const DWORD flags = TF_ES_READWRITE |
        (requireSynchronous ? TF_ES_SYNC : TF_ES_ASYNCDONTCARE);
    const HRESULT hr = context->RequestEditSession(
        service.clientId,
        editSession,
        flags,
        &sessionHr);
    if (FAILED(hr)) {
        return logEditSessionResult(service, "RequestEditSession", hr);
    }
    if (FAILED(sessionHr)) {
        logEditSessionResult(service, "DoEditSession", sessionHr);
    }
    return sessionHr;
}

HRESULT TextService::requestEditSession(ITfContext* context, const ime::engine::EngineOutput& output) {
    return requestEditSession(context, output, true);
}

HRESULT TextService::requestEditSession(
    ITfContext* context,
    const ime::engine::EngineOutput& output,
    bool requireSynchronous) {
    if (context == nullptr || clientId == TF_CLIENTID_NULL) {
        return E_UNEXPECTED;
    }

    auto editSession = yume::utils::makeComPtr<TextEditSession>(*this, context, output);
    if (editSession == nullptr) {
        return E_OUTOFMEMORY;
    }

    return EditOps::requestReadWriteEditSession(
        *this,
        context,
        static_cast<ITfEditSession*>(editSession.Get()),
        requireSynchronous);
}

HRESULT TextService::applyEngineOutput(
    TfEditCookie editCookie,
    ITfContext* context,
    const ime::engine::EngineOutput& output) {
    if (output.shouldCommitText() && output.commit.has_value()) {
        return EditOps::logEditSessionResult(*this, "commitText", commitText(editCookie, context, *output.commit));
    }

    if (output.shouldUpdateComposition() && output.composition.has_value() && output.composition->visible) {
        return EditOps::logEditSessionResult(
            *this,
            "updateComposition",
            updateComposition(editCookie, context, *output.composition));
    }

    if (output.shouldClearComposition() && output.isConsumed) {
        return EditOps::logEditSessionResult(*this, "clearComposition", clearComposition(editCookie));
    }

    return S_OK;
}

}
