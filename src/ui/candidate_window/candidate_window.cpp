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



#include "ui/candidate_window/candidate_window.hpp"

#include "ui/candidate_window/candidate_window_theme.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace yume::ui::candidate_window {

namespace {

constexpr wchar_t kCandidateWindowClassName[] = L"YumeCandidateWindow";
constexpr int kDefaultPageSize = 6;
constexpr int kAnchorMargin = 6;

HMODULE currentModuleHandle() {
    HMODULE moduleHandle = nullptr;
    const BOOL loaded = GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&CandidateWindow::windowProc),
        &moduleHandle);
    return (loaded != FALSE) ? moduleHandle : nullptr;
}

RECT workAreaForAnchorRect(const RECT& anchorRect) {
    const POINT anchorPoint{
        anchorRect.left,
        anchorRect.bottom,
    };
    const HMONITOR monitor = MonitorFromPoint(anchorPoint, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor != nullptr && GetMonitorInfoW(monitor, &monitorInfo) != FALSE) {
        return monitorInfo.rcWork;
    }

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    return workArea;
}

}

CandidateWindow::~CandidateWindow() {
    windowHandle.reset();
    bodyFontHandle.reset();
    chromeFontHandle.reset();
    if (const HMODULE moduleHandle = currentModuleHandle(); moduleHandle != nullptr) {
        UnregisterClassW(kCandidateWindowClassName, reinterpret_cast<HINSTANCE>(moduleHandle));
    }
}

void CandidateWindow::show(const CandidateUIModel& nextModel, const RECT& anchorRect, HWND ownerWindow) {
    if (!nextModel.isVisible || nextModel.candidates.empty()) {
        hide();
        return;
    }

    ensureWindow(ownerWindow);
    if (!windowHandle) {
        return;
    }

    model = nextModel;
    model.normalize();
    updateBounds(anchorRect);
    InvalidateRect(windowHandle.get(), nullptr, TRUE);
    ShowWindow(windowHandle.get(), SW_SHOWNOACTIVATE);
}

void CandidateWindow::hide() {
    model = {};
    if (windowHandle) {
        ShowWindow(windowHandle.get(), SW_HIDE);
    }
}

LRESULT CALLBACK CandidateWindow::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* createStruct = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
    }

    auto* self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self == nullptr) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    switch (message) {
        case WM_PAINT: {
            yume::utils::ScopedPaint paint(hwnd);
            self->paint(paint.get());
            return 0;
        }
        case WM_ERASEBKGND:
            return TRUE;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void CandidateWindow::ensureWindow(HWND ownerWindow) {
    if (windowHandle) {
        return;
    }

    const HINSTANCE instance = reinterpret_cast<HINSTANCE>(currentModuleHandle());
    if (instance == nullptr) {
        return;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &CandidateWindow::windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    windowClass.lpszClassName = kCandidateWindowClassName;
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    RegisterClassExW(&windowClass);

    windowHandle.reset(CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kCandidateWindowClassName,
        L"Yume Candidates",
        WS_POPUP,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kMinWindowWidth,
        kItemHeight,
        ownerWindow,
        nullptr,
        instance,
        this));

    LOGFONTW bodyFont{};
    bodyFont.lfHeight = -20;
    bodyFont.lfWeight = FW_MEDIUM;
    bodyFont.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(bodyFont.lfFaceName, L"Yu Gothic UI");
    bodyFontHandle.reset(CreateFontIndirectW(&bodyFont));

    LOGFONTW chromeFont{};
    chromeFont.lfHeight = -13;
    chromeFont.lfWeight = FW_SEMIBOLD;
    chromeFont.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(chromeFont.lfFaceName, L"Segoe UI Variable Text");
    chromeFontHandle.reset(CreateFontIndirectW(&chromeFont));
}

void CandidateWindow::updateBounds(const RECT& anchorRect) {
    if (!windowHandle) {
        return;
    }

    const int windowWidth = computeWindowWidth();
    const int windowHeight = computeWindowHeight();
    const RECT workArea = workAreaForAnchorRect(anchorRect);
    yume::utils::UniqueGdiObject region(CreateRoundRectRgn(
        0,
        0,
        windowWidth + 1,
        windowHeight + 1,
        kCornerRadius,
        kCornerRadius));
    if (::SetWindowRgn(windowHandle.get(), reinterpret_cast<HRGN>(region.get()), TRUE)) {
        region.release();
    }

    int windowX = anchorRect.left;
    int windowY = anchorRect.bottom + kAnchorMargin;

    if (windowX + windowWidth > workArea.right) {
        windowX = (std::max)(workArea.left, workArea.right - windowWidth);
    }
    if (windowX < workArea.left) {
        windowX = workArea.left;
    }

    if (windowY + windowHeight > workArea.bottom) {
        windowY = anchorRect.top - kAnchorMargin - windowHeight;
    }
    if (windowY < workArea.top) {
        windowY = workArea.top;
    }

    SetWindowPos(
        windowHandle.get(),
        HWND_TOP,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void CandidateWindow::paint(HDC deviceContext) const {
    RECT clientRect{};
    GetClientRect(windowHandle.get(), &clientRect);
    const auto& palette = paletteFor(model.theme);

    yume::utils::UniqueGdiObject backgroundBrush(CreateSolidBrush(palette.windowBackground));
    FillRect(deviceContext, &clientRect, reinterpret_cast<HBRUSH>(backgroundBrush.get()));

    RECT insetRect{
        clientRect.left + 1,
        clientRect.top + 1,
        clientRect.right - 1,
        clientRect.bottom - 1,
    };
    yume::utils::UniqueGdiObject insetBrush(CreateSolidBrush(palette.windowInset));
    FillRect(deviceContext, &insetRect, reinterpret_cast<HBRUSH>(insetBrush.get()));

    RECT headerFillRect{
        clientRect.left + 1,
        clientRect.top + 1,
        clientRect.right - 1,
        clientRect.top + kHeaderHeight,
    };
    yume::utils::UniqueGdiObject headerBrush(CreateSolidBrush(palette.headerBackground));
    FillRect(deviceContext, &headerFillRect, reinterpret_cast<HBRUSH>(headerBrush.get()));

    yume::utils::UniqueGdiObject borderPen(CreatePen(PS_SOLID, 1, palette.windowBorder));
    yume::utils::ScopedSelectedObject selectedPen(deviceContext, borderPen.get());
    yume::utils::ScopedSelectedObject selectedBrush(deviceContext, GetStockObject(HOLLOW_BRUSH));
    RoundRect(
        deviceContext,
        clientRect.left,
        clientRect.top,
        clientRect.right,
        clientRect.bottom,
        kCornerRadius,
        kCornerRadius);

    SetBkMode(deviceContext, TRANSPARENT);
    if (chromeFontHandle) {
        yume::utils::ScopedSelectedObject selectedChromeFont(deviceContext, chromeFontHandle.get());
        const int currentPage = model.pageSize > 0 ? (model.selectedIndex / model.pageSize) + 1 : 1;

        RECT headerRect{
            clientRect.left + kPadding,
            clientRect.top + 9,
            clientRect.right - kPadding,
            clientRect.top + 24,
        };
        SetTextColor(deviceContext, palette.headerText);
        std::wstring metaText =
            L"Candidates " + std::to_wstring(model.selectedIndex + 1) + L" / " +
            std::to_wstring(model.candidates.size());
        DrawTextW(deviceContext, metaText.c_str(), static_cast<int>(metaText.size()), &headerRect, DT_LEFT | DT_TOP | DT_SINGLELINE);

        RECT pageRect{
            clientRect.left + kPadding,
            clientRect.top + 9,
            clientRect.right - kPadding,
            clientRect.top + 24,
        };
        SetTextColor(deviceContext, palette.secondaryText);
        std::wstring pageText = L"Page " + std::to_wstring(currentPage);
        DrawTextW(deviceContext, pageText.c_str(), static_cast<int>(pageText.size()), &pageRect, DT_RIGHT | DT_TOP | DT_SINGLELINE);
    }

    yume::utils::UniqueGdiObject dividerPen(CreatePen(PS_SOLID, 1, palette.divider));
    yume::utils::ScopedSelectedObject selectedDividerPen(deviceContext, dividerPen.get());
    MoveToEx(deviceContext, clientRect.left + kPadding, clientRect.top + kHeaderHeight, nullptr);
    LineTo(deviceContext, clientRect.right - kPadding, clientRect.top + kHeaderHeight);

    std::optional<yume::utils::ScopedSelectedObject> selectedBodyFont;
    if (bodyFontHandle) {
        selectedBodyFont.emplace(deviceContext, bodyFontHandle.get());
    }

    const int visibleItems = visibleItemCount();
    for (int row = 0; row < visibleItems; ++row) {
        const size_t index = static_cast<size_t>(model.visibleStartIndex + row);
        RECT itemRect{
            clientRect.left + kPadding,
            clientRect.top + kHeaderHeight + 8 + row * kItemHeight,
            clientRect.right - kPadding,
            clientRect.top + kHeaderHeight + 8 + (row + 1) * kItemHeight,
        };

        const bool selected = static_cast<int32_t>(index) == model.selectedIndex;
        if (selected) {
            yume::utils::UniqueGdiObject selectionBrush(CreateSolidBrush(palette.selectionBackground));
            yume::utils::ScopedSelectedObject selectedItemBrush(deviceContext, selectionBrush.get());
            yume::utils::UniqueGdiObject selectionPen(CreatePen(PS_SOLID, 1, palette.selectionBorder));
            yume::utils::ScopedSelectedObject selectedItemPen(deviceContext, selectionPen.get());
            RoundRect(deviceContext, itemRect.left, itemRect.top, itemRect.right, itemRect.bottom, 10, 10);

            RECT indicatorRect{
                itemRect.left + 8,
                itemRect.top + 9,
                itemRect.left + 11,
                itemRect.bottom - 9,
            };
            yume::utils::UniqueGdiObject indicatorBrush(CreateSolidBrush(palette.selectionIndicator));
            FillRect(deviceContext, &indicatorRect, reinterpret_cast<HBRUSH>(indicatorBrush.get()));
        }

        RECT badgeRect{
            itemRect.left + 18,
            itemRect.top + 10,
            itemRect.left + 44,
            itemRect.bottom - 10,
        };
        yume::utils::UniqueGdiObject badgeBrush(CreateSolidBrush(selected ? palette.selectionBorder : palette.badgeBackground));
        yume::utils::ScopedSelectedObject selectedBadgeBrush(deviceContext, badgeBrush.get());
        yume::utils::UniqueGdiObject badgePen(CreatePen(PS_SOLID, 1, selected ? palette.selectionBorder : palette.windowBorder));
        yume::utils::ScopedSelectedObject selectedBadgePen(deviceContext, badgePen.get());
        RoundRect(deviceContext, badgeRect.left, badgeRect.top, badgeRect.right, badgeRect.bottom, 8, 8);

        std::wstring badgeText = std::to_wstring(index + 1);
        SetTextColor(deviceContext, selected ? palette.selectionText : palette.badgeText);
        RECT badgeTextRect = badgeRect;
        DrawTextW(deviceContext, badgeText.c_str(), static_cast<int>(badgeText.size()), &badgeTextRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT textRect{
            badgeRect.right + 14,
            itemRect.top + 2,
            itemRect.right - 14,
            itemRect.bottom - 2,
        };
        SetTextColor(deviceContext, selected ? palette.selectionText : palette.bodyText);
        DrawTextW(
            deviceContext,
            model.candidates[index].c_str(),
            static_cast<int>(model.candidates[index].size()),
            &textRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (!selected && row + 1 < visibleItems) {
            yume::utils::UniqueGdiObject itemDividerPen(CreatePen(PS_SOLID, 1, palette.divider));
            yume::utils::ScopedSelectedObject selectedItemDividerPen(deviceContext, itemDividerPen.get());
            MoveToEx(deviceContext, textRect.left, itemRect.bottom - 1, nullptr);
            LineTo(deviceContext, itemRect.right - 8, itemRect.bottom - 1);
        }
    }

    RECT footerRect{
        clientRect.left + kPadding,
        clientRect.bottom - kFooterHeight,
        clientRect.right - kPadding,
        clientRect.bottom - 6,
    };
    SetTextColor(deviceContext, palette.footerText);
    const wchar_t* footer = L"Enter to select";
    DrawTextW(deviceContext, footer, -1, &footerRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

int CandidateWindow::computeWindowHeight() const {
    const long long itemsHeight = static_cast<long long>(visibleItemCount()) * kItemHeight;
    const long long totalHeight = itemsHeight + kHeaderHeight + kFooterHeight + kPadding + 12;
    return static_cast<int>(std::clamp<long long>(totalHeight, kItemHeight, 32767));
}

int CandidateWindow::computeWindowWidth() const {
    if (!windowHandle) {
        return kMinWindowWidth;
    }

    yume::utils::ScopedWindowDc deviceContext(windowHandle.get());
    if (deviceContext.get() == nullptr) {
        return kMinWindowWidth;
    }

    std::optional<yume::utils::ScopedSelectedObject> selectedBodyFont;
    if (bodyFontHandle) {
        selectedBodyFont.emplace(deviceContext.get(), bodyFontHandle.get());
    }

    SIZE textSize{};
    int widestText = 0;
    const int visibleItems = visibleItemCount();
    for (int row = 0; row < visibleItems; ++row) {
        const size_t index = static_cast<size_t>(model.visibleStartIndex + row);
        if (index >= model.candidates.size()) {
            break;
        }
        if (GetTextExtentPoint32W(
                deviceContext.get(),
                model.candidates[index].c_str(),
                static_cast<int>(model.candidates[index].size()),
                &textSize)) {
            widestText = (std::max)(widestText, static_cast<int>(textSize.cx));
        }
    }

    const int computedWidth = widestText + 110;
    return std::clamp(computedWidth, kMinWindowWidth, kMaxWindowWidth);
}

int CandidateWindow::visibleItemCount() const {
    const int pageSize = model.pageSize > 0 ? model.pageSize : kDefaultPageSize;
    const int remainingItems = static_cast<int>(model.candidates.size()) - model.visibleStartIndex;
    return (std::min)((std::max)(remainingItems, 0), pageSize);
}

}
