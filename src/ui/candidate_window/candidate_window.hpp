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
#include "utils/win_raii.hpp"

#include <windows.h>

namespace yume::ui::candidate_window {

    class CandidateWindow final {
    public:
        CandidateWindow() = default;
        ~CandidateWindow();

        void show(const CandidateUIModel& model, const RECT& anchorRect, HWND ownerWindow);
        void hide();

    private:
        static constexpr int kMinWindowWidth = 324;
        static constexpr int kMaxWindowWidth = 436;
        static constexpr int kHeaderHeight = 34;
        static constexpr int kFooterHeight = 22;
        static constexpr int kItemHeight = 42;
        static constexpr int kPadding = 16;
        static constexpr int kCornerRadius = 16;
        static constexpr int kMaxVisibleItems = 6;

        static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

        void ensureWindow(HWND ownerWindow);
        void updateBounds(const RECT& anchorRect);
        void paint(HDC deviceContext) const;
        int computeWindowHeight() const;
        int computeWindowWidth() const;
        int visibleItemCount() const;

        yume::utils::UniqueWindowHandle windowHandle;
        yume::utils::UniqueGdiObject bodyFontHandle;
        yume::utils::UniqueGdiObject chromeFontHandle;
        CandidateUIModel model;
    };

}
