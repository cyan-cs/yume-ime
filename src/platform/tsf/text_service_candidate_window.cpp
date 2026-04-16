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
#include "utils/windows_theme.hpp"

namespace yume::platform::tsf {

namespace {

constexpr int32_t kCandidatePageSize = 6;

ui::candidate_window::CandidateWindowTheme candidateWindowThemeFromAppTheme(
    yume::utils::windows_theme::AppTheme theme) {
    switch (theme) {
        case yume::utils::windows_theme::AppTheme::Dark:
            return ui::candidate_window::CandidateWindowTheme::Dark;
        case yume::utils::windows_theme::AppTheme::Light:
        default:
            return ui::candidate_window::CandidateWindowTheme::Light;
    }
}

ui::candidate_window::CandidateUIModel buildCandidateUiModel(
    const ime::engine::CandidateList& candidates) {
    ui::candidate_window::CandidateUIModel model;
    model.isVisible = true;
    model.selectedIndex = candidates.selectedIndex;
    model.pageSize = kCandidatePageSize;
    model.visibleStartIndex = (model.selectedIndex / model.pageSize) * model.pageSize;
    model.theme = candidateWindowThemeFromAppTheme(yume::utils::windows_theme::queryAppTheme());
    model.candidates.reserve(candidates.items->size());
    for (const auto& candidate : *candidates.items) {
        model.candidates.emplace_back(candidate.text.begin(), candidate.text.end());
    }
    return model;
}

}
void TextService::updateCandidateUi(ITfContext* context, const ime::engine::EngineOutput& output) {
    if (output.shouldCloseCandidates() || !output.candidates.has_value() || !output.candidates->items ||
        output.candidates->items->empty()) {
        DispatchOps::closeCandidateUi(*this);
        return;
    }

    const auto model = buildCandidateUiModel(*output.candidates);
    const HRESULT uiElementHr = ensureCandidateUiElement(context, model);
    if (FAILED(uiElementHr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "ensureCandidateUiElement", uiElementHr);
        DispatchOps::closeCandidateUi(*this);
        return;
    }

    syncCandidateWindowVisibilityFromUiElement();
    if (!isCandidateUiShown()) {
        hideCandidateWindow();
        return;
    }

    const HWND ownerWindow = helpers::getActiveViewWindow(context);
    const auto anchorRect = resolveCandidateAnchorRect(context);
    if (!anchorRect.has_value() || ownerWindow == nullptr) {
        hideCandidateWindow();
        return;
    }

    candidateWindow.show(model, *anchorRect, ownerWindow);
}

std::optional<RECT> TextService::resolveCandidateAnchorRect(ITfContext* context) const {
    if (hasLastKnownAnchorRect) {
        return lastKnownAnchorRect;
    }

    if (context != nullptr) {
        const HWND ownerWindow = helpers::getActiveViewWindow(context);
        if (ownerWindow != nullptr) {
            RECT windowRect{};
            if (GetWindowRect(ownerWindow, &windowRect)) {
                return RECT{
                    windowRect.left + 24,
                    windowRect.bottom - 60,
                    windowRect.left + 264,
                    windowRect.bottom - 36,
                };
            }
        }
    }

    return std::nullopt;
}

void TextService::hideCandidateWindow() {
    candidateWindow.hide();
}

void TextService::syncCandidateWindowVisibilityFromUiElement() {
    if (!isCandidateUiShown()) {
        hideCandidateWindow();
    }
}

bool TextService::isCandidateUiShown() const {
    if (candidateUiElement == nullptr) {
        return false;
    }

    BOOL shown = TRUE;
    return SUCCEEDED(candidateUiElement->IsShown(&shown)) && shown != FALSE;
}

}
