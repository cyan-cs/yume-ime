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

HRESULT TextService::EditOps::insertTextAtSelection(
    const TextService& service,
    TfEditCookie editCookie,
    ITfContext* context,
    std::u16string_view text,
    ITfRange** insertedRange) {
    if (context == nullptr) {
        return E_INVALIDARG;
    }
    if (insertedRange != nullptr) {
        *insertedRange = nullptr;
    }

    Microsoft::WRL::ComPtr<ITfInsertAtSelection> insertAtSelection;
    HRESULT hr = helpers::queryInterface(context, insertAtSelection);
    if (FAILED(hr)) {
        return logEditSessionResult(service, "QueryInterface(ITfInsertAtSelection)", hr);
    }

    const DWORD insertFlags = (insertedRange == nullptr) ? TF_IAS_NOQUERY : 0;
    hr = insertAtSelection->InsertTextAtSelection(
        editCookie,
        insertFlags,
        reinterpret_cast<const WCHAR*>(text.data()),
        static_cast<LONG>(text.size()),
        insertedRange);
    if (FAILED(hr)) {
        return logEditSessionResult(service, "InsertTextAtSelection", hr);
    }

    return S_OK;
}

HRESULT TextService::commitText(
    TfEditCookie editCookie,
    ITfContext* context,
    std::u16string_view text) {
    if (context == nullptr) {
        return E_INVALIDARG;
    }

    if (activeComposition != nullptr && activeCompositionContext.Get() == context) {
        Microsoft::WRL::ComPtr<ITfRange> compositionRange;
        HRESULT hr = EditOps::getActiveCompositionRange(*this, compositionRange.GetAddressOf());
        if (FAILED(hr)) {
            releaseActiveComposition();
        } else {
            hr = compositionRange->SetText(
                editCookie,
                0,
                reinterpret_cast<const WCHAR*>(text.data()),
                static_cast<LONG>(text.size()));
            if (FAILED(hr)) {
                releaseActiveComposition();
                return hr;
            }

            const HRESULT selectionHr = helpers::setCollapsedSelectionAtOffset(
                editCookie,
                context,
                compositionRange.Get(),
                text.size(),
                false);
            if (FAILED(selectionHr)) {
                YUME_LOG_HRESULT(
                    yume::utils::Logger::Level::Warn,
                    "TextService",
                    "SetSelection(commit composition)",
                    selectionHr);
            }

            hr = activeComposition->EndComposition(editCookie);
            releaseActiveComposition();
            return hr;
        }
    }

    HRESULT hr = EditOps::insertTextAtSelection(
        *this,
        editCookie,
        context,
        text,
        nullptr);
    if (FAILED(hr)) {
        return hr;
    }

    return S_OK;
}

}
