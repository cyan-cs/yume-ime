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
#include "platform/tsf/tsf_helpers.hpp"

#include "utils/com_ptr.hpp"
#include "utils/logger.hpp"

namespace yume::platform::tsf {

TextService::UiElementRegistration::UiElementRegistration(UiElementRegistration&& other) noexcept
    : manager(std::move(other.manager))
    , id(other.id) {
    other.id = TF_INVALID_UIELEMENTID;
}

TextService::UiElementRegistration& TextService::UiElementRegistration::operator=(
    UiElementRegistration&& other) noexcept {
    if (this != &other) {
        reset();
        manager = std::move(other.manager);
        id = other.id;
        other.id = TF_INVALID_UIELEMENTID;
    }
    return *this;
}

TextService::UiElementRegistration::~UiElementRegistration() {
    reset();
}

void TextService::UiElementRegistration::clear() {
    id = TF_INVALID_UIELEMENTID;
    manager.Reset();
}

void TextService::UiElementRegistration::reset() {
    auto ownedManager = manager;
    const DWORD ownedId = id;
    clear();

    if (ownedManager != nullptr && ownedId != TF_INVALID_UIELEMENTID) {
        ownedManager->EndUIElement(ownedId);
    }
}

ITfDocumentMgr* TextService::resolveFocusedDocumentManager() const {
    return focusedDocumentManager.Get();
}

HRESULT TextService::ensureCandidateUiElement(
    ITfContext* context,
    const ui::candidate_window::CandidateUIModel& model) {
    (void)context;

    if (threadManager == nullptr) {
        return E_UNEXPECTED;
    }

    Microsoft::WRL::ComPtr<ITfUIElementMgr> uiElementMgr;
    HRESULT hr = helpers::queryInterface(threadManager.Get(), uiElementMgr);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "QueryInterface(ITfUIElementMgr)", hr);
        return hr;
    }

    if (candidateUiElement == nullptr) {
        candidateUiElement = yume::utils::makeComPtr<CandidateUiElement>(this);
        if (candidateUiElement == nullptr) {
            return E_OUTOFMEMORY;
        }
    }

    candidateUiElement->update(model, resolveFocusedDocumentManager());

    if (!candidateUiRegistration.isActive()) {
        BOOL show = TRUE;
        DWORD uiElementId = TF_INVALID_UIELEMENTID;
        hr = uiElementMgr->BeginUIElement(candidateUiElement.Get(), &show, &uiElementId);
        if (FAILED(hr)) {
            YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "BeginUIElement", hr);
            return hr;
        }

        candidateUiRegistration.manager = uiElementMgr;
        candidateUiRegistration.id = uiElementId;
        candidateUiElement->Show(show);
        return S_OK;
    }

    hr = uiElementMgr->UpdateUIElement(candidateUiRegistration.id);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Warn, "TextService", "UpdateUIElement", hr);
        candidateUiRegistration.reset();
        BOOL show = TRUE;
        DWORD uiElementId = TF_INVALID_UIELEMENTID;
        hr = uiElementMgr->BeginUIElement(candidateUiElement.Get(), &show, &uiElementId);
        if (FAILED(hr)) {
            YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "BeginUIElement(recover)", hr);
            return hr;
        }

        candidateUiRegistration.manager = uiElementMgr;
        candidateUiRegistration.id = uiElementId;
        candidateUiElement->Show(show);
    }

    return hr;
}

void TextService::endCandidateUiElement() {
    candidateUiRegistration.reset();
    resetCandidateUiState();
}

void TextService::resetCandidateUiState() {
    if (candidateUiElement != nullptr) {
        candidateUiElement->clear();
        candidateUiElement.Reset();
    }
}

}
