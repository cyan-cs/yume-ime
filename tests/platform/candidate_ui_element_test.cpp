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


#include <gtest/gtest.h>

#include "platform/tsf/candidate_ui_element.hpp"
#include "utils/com_ptr.hpp"

#include <functional>
#include <wrl/client.h>

namespace {

Microsoft::WRL::ComPtr<yume::platform::tsf::CandidateUiElement> makeElement(
    yume::platform::tsf::CandidateUiElementOwner* owner) {
    return yume::utils::makeComPtr<yume::platform::tsf::CandidateUiElement>(owner);
}

class FakeCandidateUiOwner final : public yume::platform::tsf::CandidateUiElementOwner {
public:
    bool allowSelectionChange = true;
    bool finalizeRequested = false;
    bool abortRequested = false;
    std::function<void(int32_t)> onSelectionChanged;

    bool onCandidateUiSelectionChanged(int32_t index) override {
        if (onSelectionChanged) {
            onSelectionChanged(index);
        }
        return allowSelectionChange;
    }
    void onCandidateUiFinalizeRequested() override { finalizeRequested = true; }
    void onCandidateUiAbortRequested() override { abortRequested = true; }
};

yume::ui::candidate_window::CandidateUIModel buildModel() {
    yume::ui::candidate_window::CandidateUIModel model;
    model.isVisible = true;
    model.pageSize = 3;
    model.selectedIndex = 4;
    model.visibleStartIndex = 3;
    model.candidates = {
        L"one",
        L"two",
        L"three",
        L"four",
        L"five",
        L"six",
        L"seven",
    };
    return model;
}

}

TEST(CandidateUiElementTest, CurrentPageFollowsVisibleStartIndex) {
    auto element = makeElement(nullptr);
    element->update(buildModel(), nullptr);

    UINT page = 0;
    EXPECT_EQ(element->GetCurrentPage(&page), S_OK);
    EXPECT_EQ(page, 1u);
}

TEST(CandidateUiElementTest, SetSelectionMovesVisiblePageWindow) {
    auto element = makeElement(nullptr);
    auto model = buildModel();
    model.selectedIndex = 0;
    model.visibleStartIndex = 0;
    element->update(model, nullptr);

    EXPECT_EQ(element->SetSelection(5), S_OK);

    UINT page = 0;
    UINT selection = 0;
    EXPECT_EQ(element->GetCurrentPage(&page), S_OK);
    EXPECT_EQ(element->GetSelection(&selection), S_OK);
    EXPECT_EQ(page, 1u);
    EXPECT_EQ(selection, 5u);
}

TEST(CandidateUiElementTest, SetSelectionWrapsPastLastCandidateToFirst) {
    auto element = makeElement(nullptr);
    auto model = buildModel();
    model.selectedIndex = 6;
    model.visibleStartIndex = 6;
    element->update(model, nullptr);

    EXPECT_EQ(element->SetSelection(7), S_OK);

    UINT page = 0;
    UINT selection = 0;
    EXPECT_EQ(element->GetCurrentPage(&page), S_OK);
    EXPECT_EQ(element->GetSelection(&selection), S_OK);
    EXPECT_EQ(page, 0u);
    EXPECT_EQ(selection, 0u);
}

TEST(CandidateUiElementTest, SetPageIndexMovesToFirstItemOnRequestedPage) {
    auto element = makeElement(nullptr);
    auto model = buildModel();
    model.selectedIndex = 1;
    model.visibleStartIndex = 0;
    element->update(model, nullptr);

    UINT pageIndex[] = {0, 2, 5};
    EXPECT_EQ(element->SetPageIndex(pageIndex, 3), S_OK);

    UINT page = 0;
    UINT selection = 0;
    EXPECT_EQ(element->GetCurrentPage(&page), S_OK);
    EXPECT_EQ(element->GetSelection(&selection), S_OK);
    EXPECT_EQ(page, 0u);
    EXPECT_EQ(selection, 1u);

    EXPECT_EQ(element->SetSelection(4), S_OK);
    EXPECT_EQ(element->SetPageIndex(pageIndex, 3), S_OK);
    EXPECT_EQ(element->GetCurrentPage(&page), S_OK);
    EXPECT_EQ(element->GetSelection(&selection), S_OK);
    EXPECT_EQ(page, 1u);
    EXPECT_EQ(selection, 4u);
}

TEST(CandidateUiElementTest, SetSelectionFailsWhenOwnerRejectsSelectionChange) {
    FakeCandidateUiOwner owner;
    owner.allowSelectionChange = false;
    auto element = makeElement(&owner);
    auto model = buildModel();
    model.selectedIndex = 1;
    model.visibleStartIndex = 0;
    element->update(model, nullptr);

    UINT selection = 0;
    EXPECT_EQ(element->SetSelection(5), E_FAIL);
    EXPECT_EQ(element->GetSelection(&selection), S_OK);
    EXPECT_EQ(selection, 1u);
    EXPECT_FALSE(owner.finalizeRequested);
    EXPECT_FALSE(owner.abortRequested);
}

TEST(CandidateUiElementTest, SetSelectionDoesNotOverwriteOwnerDrivenModelUpdate) {
    FakeCandidateUiOwner owner;
    auto element = makeElement(&owner);
    auto model = buildModel();
    model.selectedIndex = 1;
    model.visibleStartIndex = 0;
    element->update(model, nullptr);

    owner.onSelectionChanged = [&](int32_t index) {
        EXPECT_EQ(index, 5);
        auto updatedModel = buildModel();
        updatedModel.selectedIndex = 2;
        updatedModel.visibleStartIndex = 0;
        updatedModel.candidates = {L"alpha", L"beta", L"gamma"};
        element->update(updatedModel, nullptr);
    };

    EXPECT_EQ(element->SetSelection(5), S_OK);

    UINT selection = 0;
    UINT count = 0;
    EXPECT_EQ(element->GetSelection(&selection), S_OK);
    EXPECT_EQ(selection, 2u);
    EXPECT_EQ(element->GetCount(&count), S_OK);
    EXPECT_EQ(count, 3u);
}

TEST(CandidateUiElementTest, SetPageIndexDoesNotReportSelectionChange) {
    auto element = makeElement(nullptr);
    auto model = buildModel();
    model.selectedIndex = 1;
    model.visibleStartIndex = 0;
    element->update(model, nullptr);

    DWORD flags = 0;
    UINT pageIndex[] = {0, 2, 5};
    EXPECT_EQ(element->SetPageIndex(pageIndex, 3), S_OK);
    EXPECT_EQ(element->GetUpdatedFlags(&flags), S_OK);
    EXPECT_EQ(flags & TF_CLUIE_SELECTION, 0u);
    EXPECT_NE(flags & TF_CLUIE_PAGEINDEX, 0u);
    EXPECT_NE(flags & TF_CLUIE_CURRENTPAGE, 0u);
}

TEST(CandidateUiElementTest, InvalidSetPageIndexLeavesPreviousStateUntouched) {
    auto element = makeElement(nullptr);
    auto model = buildModel();
    model.selectedIndex = 4;
    model.visibleStartIndex = 3;
    element->update(model, nullptr);

    UINT beforePage = 0;
    EXPECT_EQ(element->GetCurrentPage(&beforePage), S_OK);
    EXPECT_EQ(beforePage, 1u);

    UINT invalidPageIndex[] = {0, 6, 5};
    EXPECT_EQ(element->SetPageIndex(invalidPageIndex, 3), E_INVALIDARG);

    UINT afterPage = 0;
    EXPECT_EQ(element->GetCurrentPage(&afterPage), S_OK);
    EXPECT_EQ(afterPage, 1u);
}

TEST(CandidateUiElementTest, SetPageIndexMustStartAtFirstCandidate) {
    auto element = makeElement(nullptr);
    auto model = buildModel();
    model.selectedIndex = 4;
    model.visibleStartIndex = 3;
    element->update(model, nullptr);

    UINT beforePage = 0;
    EXPECT_EQ(element->GetCurrentPage(&beforePage), S_OK);
    EXPECT_EQ(beforePage, 1u);

    UINT invalidPageIndex[] = {1, 3, 5};
    EXPECT_EQ(element->SetPageIndex(invalidPageIndex, 3), E_INVALIDARG);

    UINT afterPage = 0;
    EXPECT_EQ(element->GetCurrentPage(&afterPage), S_OK);
    EXPECT_EQ(afterPage, 1u);
}

TEST(CandidateUiElementTest, UpdateClampsInvalidSelectionAndPageSize) {
    auto element = makeElement(nullptr);
    auto model = buildModel();
    model.selectedIndex = 99;
    model.visibleStartIndex = -10;
    model.pageSize = 0;
    element->update(model, nullptr);

    UINT selection = 0;
    UINT page = 0;
    EXPECT_EQ(element->GetSelection(&selection), S_OK);
    EXPECT_EQ(element->GetCurrentPage(&page), S_OK);
    EXPECT_EQ(selection, 6u);
    EXPECT_EQ(page, 6u);
}
