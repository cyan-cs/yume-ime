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



#include "ui/candidate_window/candidate_window_theme.hpp"

namespace yume::ui::candidate_window {

const CandidateWindowThemePalette& paletteFor(CandidateWindowTheme theme) {
    static const CandidateWindowThemePalette kDarkPalette{
        RGB(49, 55, 62),
        RGB(224, 228, 233),
        RGB(244, 246, 249),
        RGB(171, 177, 185),
        RGB(71, 77, 84),
        RGB(148, 154, 161),
        RGB(34, 38, 43),
        RGB(233, 236, 240),
        RGB(153, 160, 168),
        RGB(59, 66, 75),
        RGB(96, 105, 116),
        RGB(185, 191, 198),
        RGB(247, 249, 251),
        RGB(30, 34, 39),
        RGB(74, 81, 89),
        RGB(36, 40, 46),
    };
    static const CandidateWindowThemePalette kLightPalette{
        RGB(240, 243, 246),
        RGB(98, 106, 116),
        RGB(26, 30, 35),
        RGB(116, 124, 134),
        RGB(227, 232, 237),
        RGB(126, 133, 142),
        RGB(247, 248, 250),
        RGB(56, 62, 70),
        RGB(125, 132, 141),
        RGB(231, 235, 240),
        RGB(197, 205, 215),
        RGB(124, 132, 142),
        RGB(20, 24, 29),
        RGB(244, 246, 248),
        RGB(211, 216, 222),
        RGB(255, 255, 255),
    };

    return (theme == CandidateWindowTheme::Dark) ? kDarkPalette : kLightPalette;
}

}
