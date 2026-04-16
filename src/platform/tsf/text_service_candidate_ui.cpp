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

namespace yume::platform::tsf {

bool TextService::applyUiDrivenOutput(
    const std::optional<ime::engine::EngineOutput>& output,
    const ime::engine::ImeEngine::SessionSnapshot* snapshot) {
    if (!output.has_value()) {
        return false;
    }

    const auto ownedSnapshot = engine.captureSessionSnapshot();
    const auto& rollbackSnapshot = (snapshot != nullptr) ? *snapshot : ownedSnapshot;
    ITfContext* context = resolveContext(nullptr);
    if (context == nullptr) {
        engine.restoreSessionSnapshot(rollbackSnapshot);
        DispatchOps::closeCandidateUi(*this);
        return false;
    }

    return DispatchOps::tryDispatchOutput(
        *this,
        context,
        *output,
        rollbackSnapshot,
        "dispatchEngineOutput failed for ui-driven output");
}

HRESULT STDMETHODCALLTYPE TextService::BeginUIElement(DWORD uiElementId, BOOL* show) {
    if (show == nullptr) {
        return E_POINTER;
    }

    if (uiElementId == candidateUiRegistration.id) {
        *show = TRUE;
        syncCandidateWindowVisibilityFromUiElement();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::UpdateUIElement(DWORD uiElementId) {
    if (uiElementId == candidateUiRegistration.id) {
        syncCandidateWindowVisibilityFromUiElement();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::EndUIElement(DWORD uiElementId) {
    if (uiElementId == candidateUiRegistration.id) {
        candidateUiRegistration.clear();
        resetCandidateUiState();
        hideCandidateWindow();
    }
    return S_OK;
}

bool TextService::onCandidateUiSelectionChanged(int32_t index) {
    const auto snapshot = engine.captureSessionSnapshot();
    const auto output = engine.setCandidateSelection(index);
    if (!output.has_value()) {
        return false;
    }

    return applyUiDrivenOutput(output, &snapshot);
}

void TextService::onCandidateUiFinalizeRequested() {
    const auto snapshot = engine.captureSessionSnapshot();
    applyUiDrivenOutput(engine.finalizeCandidateSelection(), &snapshot);
}

void TextService::onCandidateUiAbortRequested() {
    const auto snapshot = engine.captureSessionSnapshot();
    applyUiDrivenOutput(engine.abortCandidateSelection(), &snapshot);
}

}
