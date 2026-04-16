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

namespace yume::platform::tsf {

void TextService::releaseFocusedContext() {
    focusedContext.Reset();
}

void TextService::releaseActiveComposition() {
    hasLastKnownAnchorRect = false;
    activeComposition.Reset();
    activeCompositionContext.Reset();
}

void TextService::resetFocusTracking() {
    focusedDocumentManager.Reset();
    releaseFocusedContext();
}

void TextService::setFocusedContext(ITfContext* context) {
    focusedContext = context;
}

void TextService::setActiveComposition(ITfContext* context, ITfComposition* composition) {
    activeCompositionContext = context;
    activeComposition = composition;
}

ITfContext* TextService::resolveContext(ITfContext* preferredContext) const {
    if (preferredContext != nullptr) {
        return preferredContext;
    }
    return focusedContext.Get();
}

Microsoft::WRL::ComPtr<ITfContext> TextService::getTopContext(ITfDocumentMgr* documentMgr) const {
    Microsoft::WRL::ComPtr<ITfContext> context;
    if (documentMgr != nullptr) {
        documentMgr->GetTop(context.GetAddressOf());
    }
    return context;
}

void TextService::updateFocusedDocumentManager(ITfDocumentMgr* documentMgr) {
    if (focusedDocumentManager.Get() != documentMgr) {
        releaseActiveComposition();
    }
    focusedDocumentManager = documentMgr;
    DispatchOps::closeCandidateUi(*this);
    releaseFocusedContext();

    auto context = getTopContext(documentMgr);
    if (context != nullptr) {
        setFocusedContext(context.Get());
        adviseCompositionSink(context.Get());
    }
}

void TextService::clearTrackedContext(ITfContext* context) {
    if (focusedContext.Get() == context) {
        releaseFocusedContext();
    }
    if (activeCompositionContext.Get() == context) {
        releaseActiveComposition();
    }
}

}
