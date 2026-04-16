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

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace yume::ui::candidate_window {

    enum class CandidateWindowTheme {
        Light,
        Dark,
    };

    // Pure data structure representing the rendering view for the Candidate Window.
    // The Core Engine passes this to the UI layer, decoupling logic from rendering.
    struct CandidateUIModel {
        std::vector<std::wstring> candidates; // The list of candidates (UTF-16)
        int32_t selectedIndex;                // The currently highlighted candidate index
        int32_t visibleStartIndex;            // The first candidate shown in the current viewport
        int32_t pageSize;                     // The number of candidates shown in one page
        bool isVisible;                       // Whether the UI should currently be shown
        CandidateWindowTheme theme = CandidateWindowTheme::Light;

        CandidateUIModel() : selectedIndex(0), visibleStartIndex(0), pageSize(6), isVisible(false) {}

        void normalize() {
            const int32_t normalizedPageSize = pageSize > 0 ? pageSize : 1;
            pageSize = normalizedPageSize;

            if (candidates.empty()) {
                selectedIndex = 0;
                visibleStartIndex = 0;
                return;
            }

            const int32_t lastIndex = static_cast<int32_t>(candidates.size() - 1);
            selectedIndex = std::clamp(selectedIndex, 0, lastIndex);

            const int32_t maxVisibleStart = (std::max)(0, static_cast<int32_t>(candidates.size()) - pageSize);
            visibleStartIndex = std::clamp(visibleStartIndex, 0, maxVisibleStart);
            if (selectedIndex < visibleStartIndex || selectedIndex >= visibleStartIndex + pageSize) {
                visibleStartIndex = (selectedIndex / pageSize) * pageSize;
            }
        }
    };

}
