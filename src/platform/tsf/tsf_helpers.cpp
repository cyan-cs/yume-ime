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



#include "platform/tsf/tsf_helpers.hpp"

#include <algorithm>
#include <limits>

namespace yume::platform::tsf::helpers {

HRESULT setCollapsedSelectionAtOffset(
    TfEditCookie editCookie,
    ITfContext* context,
    ITfRange* baseRange,
    size_t offset,
    bool interim) {
    if (context == nullptr || baseRange == nullptr) {
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<ITfRange> caretRange;
    HRESULT hr = baseRange->Clone(caretRange.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }

    hr = caretRange->Collapse(editCookie, TF_ANCHOR_START);
    if (FAILED(hr)) {
        return hr;
    }

    const LONG clampedOffset = static_cast<LONG>((std::min)(
        offset,
        static_cast<size_t>((std::numeric_limits<LONG>::max)())));
    if (clampedOffset > 0) {
        LONG shifted = 0;
        hr = caretRange->ShiftStart(editCookie, clampedOffset, &shifted, nullptr);
        if (FAILED(hr)) {
            return hr;
        }
    }

    TF_SELECTION selection{};
    selection.range = caretRange.Get();
    selection.style.ase = static_cast<TfActiveSelEnd>(TS_AE_NONE);
    selection.style.fInterimChar = interim ? TRUE : FALSE;
    return context->SetSelection(editCookie, 1, &selection);
}

}
