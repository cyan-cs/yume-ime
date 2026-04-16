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
#include "platform/tsf/tsf_helpers.hpp"

#include "utils/logger.hpp"

namespace yume::platform::tsf {

namespace {

HRESULT getSelectionRange(TfEditCookie editCookie, ITfContext* context, ITfRange** range) {
    if (range == nullptr) {
        return E_POINTER;
    }
    *range = nullptr;

    TF_SELECTION selection{};
    ULONG fetched = 0;
    const HRESULT selectionHr = context->GetSelection(editCookie, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
    if (SUCCEEDED(selectionHr) && fetched > 0 && selection.range != nullptr) {
        *range = selection.range;
        return S_OK;
    }

    return context->GetStart(editCookie, range);
}

HRESULT queryInsertionRange(
    const TextService& service,
    TfEditCookie editCookie,
    ITfContext* context,
    ITfRange** range) {
    if (range == nullptr) {
        return E_POINTER;
    }
    *range = nullptr;

    Microsoft::WRL::ComPtr<ITfInsertAtSelection> insertAtSelection;
    HRESULT hr = helpers::queryInterface(context, insertAtSelection);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(
            yume::utils::Logger::Level::Error,
            "TextService",
            "QueryInterface(ITfInsertAtSelection)",
            hr);
        return hr;
    }

    hr = insertAtSelection->InsertTextAtSelection(editCookie, TF_IAS_QUERYONLY, nullptr, 0, range);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(
            yume::utils::Logger::Level::Error,
            "TextService",
            "InsertTextAtSelection(TF_IAS_QUERYONLY)",
            hr);
        return hr;
    }

    (void)service;
    return S_OK;
}

}
HRESULT TextService::EditOps::getActiveCompositionRange(const TextService& service, ITfRange** range) {
    if (range == nullptr) {
        return E_POINTER;
    }
    *range = nullptr;

    if (service.activeComposition == nullptr) {
        return E_UNEXPECTED;
    }

    return logEditSessionResult(
        service,
        "ITfComposition::GetRange",
        service.activeComposition->GetRange(range));
}

HRESULT TextService::EditOps::ensureActiveComposition(
    TextService& service,
    TfEditCookie editCookie,
    ITfContext* context,
    ITfRange** range) {
    if (range == nullptr) {
        return E_POINTER;
    }
    *range = nullptr;

    if (context == nullptr) {
        return E_INVALIDARG;
    }

    if (service.activeCompositionContext != nullptr && service.activeCompositionContext.Get() != context) {
        service.releaseActiveComposition();
    }

    if (service.activeComposition == nullptr) {
        Microsoft::WRL::ComPtr<ITfRange> initialRange;
        HRESULT hr = queryInsertionRange(service, editCookie, context, initialRange.GetAddressOf());
        if (FAILED(hr)) {
            hr = getSelectionRange(editCookie, context, initialRange.GetAddressOf());
            if (FAILED(hr)) {
                return logEditSessionResult(service, "getSelectionRange", hr);
            }
        }

        Microsoft::WRL::ComPtr<ITfContextComposition> contextComposition;
        hr = helpers::queryInterface(context, contextComposition);
        if (FAILED(hr)) {
            return logEditSessionResult(service, "QueryInterface(ITfContextComposition)", hr);
        }

        Microsoft::WRL::ComPtr<ITfComposition> composition;
        hr = contextComposition->StartComposition(
            editCookie,
            initialRange.Get(),
            static_cast<ITfCompositionSink*>(&service),
            composition.GetAddressOf());
        if (FAILED(hr)) {
            return logEditSessionResult(service, "StartComposition", hr);
        }

        service.setActiveComposition(context, composition.Get());
    }

    return getActiveCompositionRange(service, range);
}

void TextService::EditOps::updateLastKnownAnchorRect(
    TextService& service,
    TfEditCookie editCookie,
    ITfContext* context,
    ITfRange* range) {
    if (context == nullptr || range == nullptr) {
        return;
    }

    Microsoft::WRL::ComPtr<ITfContextView> view;
    if (FAILED(helpers::getActiveView(context, view)) || view == nullptr) {
        return;
    }

    RECT textRect{};
    BOOL clipped = FALSE;
    if (SUCCEEDED(view->GetTextExt(editCookie, range, &textRect, &clipped))) {
        service.lastKnownAnchorRect = textRect;
        service.hasLastKnownAnchorRect = true;
    }
}

HRESULT TextService::updateComposition(
    TfEditCookie editCookie,
    ITfContext* context,
    const ime::engine::CompositionState& compositionState) {
    if (context == nullptr) {
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<ITfRange> compositionRange;
    HRESULT hr = EditOps::ensureActiveComposition(*this, editCookie, context, compositionRange.GetAddressOf());
    if (FAILED(hr)) {
        releaseActiveComposition();
        hr = EditOps::ensureActiveComposition(*this, editCookie, context, compositionRange.GetAddressOf());
    }
    if (FAILED(hr)) {
        return hr;
    }

    hr = compositionRange->SetText(
        editCookie,
        0,
        reinterpret_cast<const WCHAR*>(compositionState.text.data()),
        static_cast<LONG>(compositionState.text.size()));
    if (FAILED(hr)) {
        return hr;
    }

    EditOps::updateLastKnownAnchorRect(*this, editCookie, context, compositionRange.Get());
    const HRESULT selectionHr = helpers::setCollapsedSelectionAtOffset(
        editCookie,
        context,
        compositionRange.Get(),
        compositionState.cursorPosition,
        true);
    if (FAILED(selectionHr)) {
        YUME_LOG_HRESULT(
            yume::utils::Logger::Level::Warn,
            "TextService",
            "SetSelection(composition)",
            selectionHr);
    }

    return S_OK;
}

HRESULT TextService::clearComposition(TfEditCookie editCookie) {
    if (activeComposition == nullptr) {
        return S_OK;
    }

    Microsoft::WRL::ComPtr<ITfRange> compositionRange;
    HRESULT hr = activeComposition->GetRange(compositionRange.GetAddressOf());
    if (SUCCEEDED(hr)) {
        const HRESULT clearHr = compositionRange->SetText(editCookie, 0, nullptr, 0);
        if (FAILED(clearHr)) {
            YUME_LOG_HRESULT(yume::utils::Logger::Level::Warn, "TextService", "SetText(clear composition)", clearHr);
        }
    } else {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Warn, "TextService", "ITfComposition::GetRange(clear)", hr);
    }

    const HRESULT endHr = activeComposition->EndComposition(editCookie);
    if (FAILED(endHr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Warn, "TextService", "EndComposition(clear)", endHr);
    }
    releaseActiveComposition();
    return SUCCEEDED(hr) ? endHr : hr;
}

}
