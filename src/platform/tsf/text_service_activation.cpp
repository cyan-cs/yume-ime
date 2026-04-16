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

#include "platform/tsf/module_state.hpp"
#include "utils/logger.hpp"

namespace yume::platform::tsf {

const CLSID kTextServiceClsid =
    {0x4e5f3910, 0x7a67, 0x45d4, {0x88, 0x9e, 0x61, 0x9c, 0x44, 0xd0, 0x15, 0x91}};

TextService::TextService()
    : objectLease(ModuleState::instance().acquireObjectLease()) {
    YUME_LOG_INFO("TextService", "construct");
}

TextService::~TextService() {
    YUME_LOG_INFO("TextService", "destruct");
    Deactivate();
}

HRESULT STDMETHODCALLTYPE TextService::Activate(ITfThreadMgr* threadMgr, TfClientId newClientId) {
    if (threadMgr == nullptr) {
        YUME_LOG_ERROR("TextService", "Activate invalid threadMgr");
        return E_INVALIDARG;
    }

    if (threadManager != nullptr) {
        return E_UNEXPECTED;
    }

    threadManager = threadMgr;
    clientId = newClientId;
    config = TextServiceConfig::loadFromFile();
    engine.processKeyEvent(ime::input::KeyEvent(
        ime::input::KeyCode::ModeHiragana,
        std::nullopt,
        true,
        false,
        false,
        false));
    YUME_LOG_INFO("TextService", "Activate clientId=", newClientId);
    sessionLease = ModuleState::instance().tryAcquireSession();
    if (!sessionLease.has_value()) {
        YUME_LOG_ERROR("TextService", "acquireSession failed");
        threadManager.Reset();
        clientId = TF_CLIENTID_NULL;
        return E_FAIL;
    }

    sessionActive = true;
    ModuleState::instance().registerTextService(this);
    const HRESULT adviseHr = adviseSinks();
    if (FAILED(adviseHr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "adviseSinks", adviseHr);
        Deactivate();
        return adviseHr;
    }

    YUME_LOG_INFO("TextService", "Activate complete");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::Deactivate() {
    YUME_LOG_INFO("TextService", "Deactivate");
    unadviseSinks();
    DispatchOps::closeCandidateUi(*this);
    releaseActiveComposition();
    resetFocusTracking();
    ModuleState::instance().unregisterTextService(this);

    if (sessionActive) {
        sessionLease.reset();
        sessionActive = false;
    }

    threadManager.Reset();
    clientId = TF_CLIENTID_NULL;
    return S_OK;
}

void TextService::serviceIdle() {
    engine.serviceIdle();
}

}
