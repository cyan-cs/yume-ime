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

#include "platform/tsf/text_service_policy.hpp"

namespace yume::platform::tsf {

HRESULT STDMETHODCALLTYPE TextService::OnSetFocus(BOOL foreground) {
    if (foreground == FALSE) {
        handleFocusLoss(resolveContext(nullptr));
        DispatchOps::closeCandidateUi(*this);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::OnInitDocumentMgr(ITfDocumentMgr* documentMgr) {
    (void)documentMgr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::OnUninitDocumentMgr(ITfDocumentMgr* documentMgr) {
    (void)documentMgr;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::OnSetFocus(
    ITfDocumentMgr* documentMgrFocus,
    ITfDocumentMgr* documentMgrPrevFocus) {
    if (documentMgrPrevFocus != nullptr && config.commitsOnFocusLoss()) {
        auto previousContext = getTopContext(documentMgrPrevFocus);
        if (previousContext != nullptr) {
            handleFocusLoss(previousContext.Get());
        }
    }

    updateFocusedDocumentManager(documentMgrFocus);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::OnPushContext(ITfContext* context) {
    const HRESULT hr = adviseCompositionSink(context);
    if (FAILED(hr)) {
        // Failing to advise ITfCompositionSink should not fail the overall
        // context push. TSF can continue operating without the extra sink, and
        // the service still has a safe fallback path.
        return S_OK;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::OnPopContext(ITfContext* context) {
    clearTrackedContext(context);
    unadviseCompositionSink(context);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::OnCompositionTerminated(
    TfEditCookie editCookie,
    ITfComposition* composition) {
    (void)composition;
    (void)editCookie;
    releaseActiveComposition();
    return S_OK;
}

void TextService::handleFocusLoss(ITfContext* context) {
    if (!policy::shouldCommitOnFocusLoss(config)) {
        return;
    }

    const auto currentState = engine.getCurrentState();
    if (currentState != ime::state::ImeState::Composing &&
        currentState != ime::state::ImeState::Converting) {
        return;
    }

    if (context == nullptr) {
        releaseActiveComposition();
        engine.discardActiveSession();
        DispatchOps::closeCandidateUi(*this);
        return;
    }

    HRESULT commitHr = S_FALSE;
    switch (engine.getCurrentState()) {
        case ime::state::ImeState::Composing: {
            const auto output = engine.processKeyEvent(
                ime::input::KeyEvent(ime::input::KeyCode::Enter, std::nullopt, true, false, false, false));
            if (output.isConsumed) {
                commitHr = dispatchEngineOutput(context, output, false);
            }
            break;
        }
        case ime::state::ImeState::Converting: {
            const auto output = engine.finalizeCandidateSelection();
            if (output.has_value()) {
                commitHr = dispatchEngineOutput(context, *output, false);
            }
            break;
        }
        default:
            break;
    }

    if (FAILED(commitHr)) {
        releaseActiveComposition();
        engine.discardActiveSession();
        DispatchOps::closeCandidateUi(*this);
    }
}

}
