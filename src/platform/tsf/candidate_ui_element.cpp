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


#include "platform/tsf/candidate_ui_element.hpp"
#include "utils/bstr.hpp"

#include <algorithm>

namespace yume::platform::tsf {

namespace {

const GUID kCandidateUiElementGuid =
    {0xb1f5f962, 0x2936, 0x4f6c, {0x91, 0x43, 0x4e, 0xb8, 0x8f, 0x78, 0x18, 0x6b}};

}
CandidateUiElement::CandidateUiElement(CandidateUiElementOwner* ownerInstance)
    : owner(ownerInstance) {}

HRESULT STDMETHODCALLTYPE CandidateUiElement::QueryInterface(REFIID riid, void** ppvObject) {
    if (riid == IID_IUnknown || riid == IID_ITfUIElement || riid == IID_ITfCandidateListUIElement ||
        riid == IID_ITfCandidateListUIElementBehavior) {
        return yume::utils::ComObjectBase<CandidateUiElement>::queryInterfacePointer(
            ppvObject,
            static_cast<ITfCandidateListUIElementBehavior*>(this));
    }

    return yume::utils::ComObjectBase<CandidateUiElement>::queryInterfacePointer(ppvObject, nullptr);
}

ULONG STDMETHODCALLTYPE CandidateUiElement::AddRef() {
    return yume::utils::ComObjectBase<CandidateUiElement>::AddRef();
}

ULONG STDMETHODCALLTYPE CandidateUiElement::Release() {
    return yume::utils::ComObjectBase<CandidateUiElement>::Release();
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::GetDescription(BSTR* description) {
    if (description == nullptr) {
        return E_POINTER;
    }
    yume::utils::ScopedBstr value(::SysAllocString(L"Yume Candidate List"));
    if (!value) {
        *description = nullptr;
        return E_OUTOFMEMORY;
    }
    *description = value.release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::GetGUID(GUID* guid) {
    if (guid == nullptr) {
        return E_POINTER;
    }
    *guid = kCandidateUiElementGuid;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::Show(BOOL show) {
    shown = (show != FALSE);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::IsShown(BOOL* show) {
    if (show == nullptr) {
        return E_POINTER;
    }
    *show = shown ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::GetUpdatedFlags(DWORD* flags) {
    if (flags == nullptr) {
        return E_POINTER;
    }
    *flags = updatedFlags;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::GetDocumentMgr(ITfDocumentMgr** documentMgrOut) {
    if (documentMgrOut == nullptr) {
        return E_POINTER;
    }
    *documentMgrOut = documentManager.Get();
    if (*documentMgrOut != nullptr) {
        (*documentMgrOut)->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::GetCount(UINT* count) {
    if (count == nullptr) {
        return E_POINTER;
    }
    *count = static_cast<UINT>(model.candidates.size());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::GetSelection(UINT* index) {
    if (index == nullptr) {
        return E_POINTER;
    }
    *index = static_cast<UINT>((std::max)(model.selectedIndex, 0));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::GetString(UINT index, BSTR* text) {
    if (text == nullptr) {
        return E_POINTER;
    }
    *text = nullptr;
    if (index >= model.candidates.size()) {
        return E_INVALIDARG;
    }

    yume::utils::ScopedBstr value(::SysAllocString(model.candidates[index].c_str()));
    if (!value) {
        return E_OUTOFMEMORY;
    }
    *text = value.release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::GetPageIndex(UINT* index, UINT size, UINT* pageCount) {
    if (pageCount == nullptr) {
        return E_POINTER;
    }
    const UINT totalPages = static_cast<UINT>(pageIndices.size());

    if (index != nullptr && size > 0) {
        const UINT copiedPages = (totalPages < size) ? totalPages : size;
        for (UINT page = 0; page < copiedPages; ++page) {
            index[page] = pageIndices[page];
        }
    }
    *pageCount = totalPages;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::SetPageIndex(UINT* index, UINT pageCount) {
    if (index == nullptr && pageCount != 0) {
        return E_POINTER;
    }
    if (pageCount == 0) {
        pageIndices.clear();
        model.visibleStartIndex = 0;
        updatedFlags = TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
        return S_OK;
    }

    const UINT totalCount = static_cast<UINT>(model.candidates.size());
    std::vector<UINT> nextPageIndices;
    nextPageIndices.reserve(pageCount);
    UINT previous = 0;
    for (UINT page = 0; page < pageCount; ++page) {
        const UINT current = index[page];
        if (current >= totalCount) {
            return E_INVALIDARG;
        }
        if (page == 0 && current != 0) {
            return E_INVALIDARG;
        }
        if (page > 0 && current <= previous) {
            return E_INVALIDARG;
        }
        nextPageIndices.push_back(current);
        previous = current;
    }

    pageIndices = std::move(nextPageIndices);
    if (!pageIndices.empty()) {
        const size_t currentPage = pageIndexForSelection();
        model.visibleStartIndex = static_cast<int32_t>(pageIndices[currentPage]);
    }
    updatedFlags = TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::GetCurrentPage(UINT* page) {
    if (page == nullptr) {
        return E_POINTER;
    }
    *page = static_cast<UINT>(pageIndexForSelection());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::SetSelection(UINT index) {
    if (model.candidates.empty()) {
        return E_UNEXPECTED;
    }
    index %= static_cast<UINT>(model.candidates.size());
    const auto previousModel = model;
    if (owner != nullptr && !owner->onCandidateUiSelectionChanged(static_cast<int32_t>(index))) {
        return E_FAIL;
    }

    const bool ownerUpdatedModel =
        model.selectedIndex != previousModel.selectedIndex ||
        model.visibleStartIndex != previousModel.visibleStartIndex ||
        model.pageSize != previousModel.pageSize ||
        model.isVisible != previousModel.isVisible ||
        model.theme != previousModel.theme ||
        model.candidates != previousModel.candidates;

    if (!ownerUpdatedModel && index < model.candidates.size()) {
        model.selectedIndex = static_cast<int32_t>(index);
        if (!pageIndices.empty()) {
            model.visibleStartIndex = static_cast<int32_t>(pageIndices[pageIndexForSelection()]);
        }
    }
    updatedFlags = TF_CLUIE_SELECTION | TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::Finalize() {
    shown = false;
    if (owner != nullptr) {
        owner->onCandidateUiFinalizeRequested();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CandidateUiElement::Abort() {
    shown = false;
    if (owner != nullptr) {
        owner->onCandidateUiAbortRequested();
    }
    return S_OK;
}

void CandidateUiElement::update(
    const yume::ui::candidate_window::CandidateUIModel& nextModel,
    ITfDocumentMgr* nextDocumentMgr) {
    model = nextModel;
    model.normalize();
    rebuildDefaultPageIndices();
    if (!pageIndices.empty()) {
        model.visibleStartIndex = static_cast<int32_t>(pageIndices[pageIndexForSelection()]);
    } else {
        model.visibleStartIndex = 0;
    }
    shown = nextModel.isVisible;
    updatedFlags = TF_CLUIE_DOCUMENTMGR | TF_CLUIE_COUNT | TF_CLUIE_SELECTION |
                   TF_CLUIE_STRING | TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
    documentManager = nextDocumentMgr;
}

void CandidateUiElement::clear() {
    model = {};
    pageIndices.clear();
    shown = false;
    updatedFlags = TF_CLUIE_DOCUMENTMGR | TF_CLUIE_COUNT | TF_CLUIE_SELECTION |
                   TF_CLUIE_STRING | TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
    documentManager.Reset();
}

size_t CandidateUiElement::pageIndexForSelection() const {
    if (pageIndices.empty() || model.selectedIndex <= 0) {
        return 0;
    }

    const UINT selection = static_cast<UINT>(model.selectedIndex);
    size_t page = 0;
    for (size_t i = 0; i < pageIndices.size(); ++i) {
        if (pageIndices[i] > selection) {
            break;
        }
        page = i;
    }
    return page;
}

void CandidateUiElement::rebuildDefaultPageIndices() {
    pageIndices.clear();
    if (model.candidates.empty()) {
        return;
    }

    const UINT pageSize = model.pageSize > 0 ? static_cast<UINT>(model.pageSize) : 1U;
    for (UINT start = 0; start < model.candidates.size(); start += pageSize) {
        pageIndices.push_back(start);
    }
}

}
