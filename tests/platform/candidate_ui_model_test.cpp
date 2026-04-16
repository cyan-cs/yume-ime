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

#include "ui/candidate_window/candidate_ui_model.hpp"

TEST(CandidateUiModelTest, NormalizeClampsSelectionAndViewport) {
    yume::ui::candidate_window::CandidateUIModel model;
    model.candidates = {L"one", L"two", L"three", L"four"};
    model.selectedIndex = 99;
    model.visibleStartIndex = -5;
    model.pageSize = 0;

    model.normalize();

    EXPECT_EQ(model.pageSize, 1);
    EXPECT_EQ(model.selectedIndex, 3);
    EXPECT_EQ(model.visibleStartIndex, 3);
}

TEST(CandidateUiModelTest, NormalizeResetsEmptyModel) {
    yume::ui::candidate_window::CandidateUIModel model;
    model.selectedIndex = 5;
    model.visibleStartIndex = 3;
    model.pageSize = 0;

    model.normalize();

    EXPECT_EQ(model.pageSize, 1);
    EXPECT_EQ(model.selectedIndex, 0);
    EXPECT_EQ(model.visibleStartIndex, 0);
}
