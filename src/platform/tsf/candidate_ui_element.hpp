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



#pragma once

#include "ui/candidate_window/candidate_ui_model.hpp"
#include "utils/com_object_base.hpp"

#include <msctf.h>
#include <windows.h>
#include <wrl/client.h>

#include <vector>

namespace yume::platform::tsf {

    class CandidateUiElementOwner {
    public:
        virtual ~CandidateUiElementOwner() = default;
        virtual bool onCandidateUiSelectionChanged(int32_t index) = 0;
        virtual void onCandidateUiFinalizeRequested() = 0;
        virtual void onCandidateUiAbortRequested() = 0;
    };

    class CandidateUiElement final
        : public yume::utils::ComObjectBase<CandidateUiElement>
        , public ITfCandidateListUIElementBehavior {
    public:
        explicit CandidateUiElement(CandidateUiElementOwner* owner);

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
        ULONG STDMETHODCALLTYPE AddRef() override;
        ULONG STDMETHODCALLTYPE Release() override;

        HRESULT STDMETHODCALLTYPE GetDescription(BSTR* description) override;
        HRESULT STDMETHODCALLTYPE GetGUID(GUID* guid) override;
        HRESULT STDMETHODCALLTYPE Show(BOOL show) override;
        HRESULT STDMETHODCALLTYPE IsShown(BOOL* show) override;

        HRESULT STDMETHODCALLTYPE GetUpdatedFlags(DWORD* flags) override;
        HRESULT STDMETHODCALLTYPE GetDocumentMgr(ITfDocumentMgr** documentMgr) override;
        HRESULT STDMETHODCALLTYPE GetCount(UINT* count) override;
        HRESULT STDMETHODCALLTYPE GetSelection(UINT* index) override;
        HRESULT STDMETHODCALLTYPE GetString(UINT index, BSTR* text) override;
        HRESULT STDMETHODCALLTYPE GetPageIndex(UINT* index, UINT size, UINT* pageCount) override;
        HRESULT STDMETHODCALLTYPE SetPageIndex(UINT* index, UINT pageCount) override;
        HRESULT STDMETHODCALLTYPE GetCurrentPage(UINT* page) override;

        HRESULT STDMETHODCALLTYPE SetSelection(UINT index) override;
        HRESULT STDMETHODCALLTYPE Finalize() override;
        HRESULT STDMETHODCALLTYPE Abort() override;

        void update(
            const yume::ui::candidate_window::CandidateUIModel& model,
            ITfDocumentMgr* documentMgr);
        void clear();

    private:
        friend class yume::utils::ComObjectBase<CandidateUiElement>;

        ~CandidateUiElement() = default;

        size_t pageIndexForSelection() const;
        void rebuildDefaultPageIndices();

        CandidateUiElementOwner* owner = nullptr;
        yume::ui::candidate_window::CandidateUIModel model;
        std::vector<UINT> pageIndices;
        Microsoft::WRL::ComPtr<ITfDocumentMgr> documentManager;
        DWORD updatedFlags = TF_CLUIE_DOCUMENTMGR | TF_CLUIE_COUNT | TF_CLUIE_SELECTION |
                             TF_CLUIE_STRING | TF_CLUIE_PAGEINDEX | TF_CLUIE_CURRENTPAGE;
        bool shown = true;
    };

}
