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

namespace {

using ime::input::KeyEvent;

bool outputMutatesDocument(const ime::engine::EngineOutput& output) {
    return output.shouldClearComposition() || output.shouldCommitText() || output.shouldUpdateComposition();
}

}

void TextService::discardSessionAndCloseUi() {
    engine.discardActiveSession();
    releaseActiveComposition();
    DispatchOps::closeCandidateUi(*this);
}

bool TextService::shouldBypassTabNavigation(WPARAM wParam) const {
    return wParam == VK_TAB &&
           !(engine.getCurrentState() == ime::state::ImeState::Composing &&
             engine.hasVisibleCandidateWindow());
}

bool TextService::shouldDiscardEmptyNonDirectSession(const KeyEvent& event) const {
    return engine.getCurrentState() != ime::state::ImeState::Direct &&
           !engine.hasActiveSession() &&
           (event.keyCode == ime::input::KeyCode::Backspace ||
            event.keyCode == ime::input::KeyCode::Delete ||
            event.keyCode == ime::input::KeyCode::Escape);
}

ime::engine::EngineOutput TextService::buildDirectCommitOutput(const KeyEvent& event) const {
    ime::engine::EngineOutput output;
    output.nextState = ime::state::ImeState::Direct;
    output.inputMode = ime::engine::InputMode::Latin;
    output.isConsumed = true;
    output.commit = buildConfiguredDirectCommitText(event);
    output.commitAction = ime::engine::CommitAction::Commit;
    output.compositionAction = ime::engine::CompositionAction::Clear;
    output.candidateAction = ime::engine::CandidateAction::Close;
    return output;
}

void TextService::tryRestartCompositionAfterCommit(ITfContext* targetContext, const KeyEvent& event) {
    if (targetContext == nullptr) {
        discardSessionAndCloseUi();
        return;
    }

    const auto snapshot = engine.captureSessionSnapshot();
    const auto restarted = engine.processKeyEvent(event);
    if (!restarted.isConsumed) {
        return;
    }

    const bool dispatched = DispatchOps::tryDispatchOutput(
        *this,
        targetContext,
        restarted,
        snapshot,
        "dispatchEngineOutput failed for restarted composition");
    if (!dispatched) {
        discardSessionAndCloseUi();
    }
}

HRESULT STDMETHODCALLTYPE TextService::OnTestKeyDown(
    ITfContext* context,
    WPARAM wParam,
    LPARAM lParam,
    BOOL* eaten) {
    (void)context;
    (void)lParam;
    if (eaten == nullptr) {
        return E_POINTER;
    }

    *eaten = shouldConsumeKey(wParam, lParam) ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::OnTestKeyUp(
    ITfContext* context,
    WPARAM wParam,
    LPARAM lParam,
    BOOL* eaten) {
    (void)context;
    (void)wParam;
    (void)lParam;
    if (eaten == nullptr) {
        return E_POINTER;
    }

    *eaten = FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::OnKeyDown(
    ITfContext* context,
    WPARAM wParam,
    LPARAM lParam,
    BOOL* eaten) {
    if (eaten == nullptr) {
        return E_POINTER;
    }

    if (shouldBypassTabNavigation(wParam)) {
        *eaten = FALSE;
        return S_OK;
    }

    KeyEvent event;
    if (!buildKeyEvent(wParam, lParam, event)) {
        *eaten = FALSE;
        return S_OK;
    }

    if (shouldDiscardEmptyNonDirectSession(event)) {
        discardSessionAndCloseUi();
        *eaten = FALSE;
        return S_OK;
    }

    ITfContext* targetContext = resolveContext(context);
    if (shouldCommitConfiguredFullWidthDirectInput(event)) {
        *eaten = TRUE;
        if (targetContext != nullptr) {
            const auto output = buildDirectCommitOutput(event);
            DispatchOps::dispatchOutputOrCloseCandidateUi(
                *this,
                targetContext,
                output,
                "dispatchEngineOutput failed for direct commit");
        } else {
            discardSessionAndCloseUi();
        }
        return S_OK;
    }

    if (shouldCommitPrintableDuringConversion(event)) {
        const HRESULT commitHr = DispatchOps::tryCommitOpenSession(*this, targetContext);
        if (SUCCEEDED(commitHr)) {
            *eaten = TRUE;
            tryRestartCompositionAfterCommit(targetContext, event);
            return S_OK;
        }
    }

    if (shouldCommitEscapeDuringConversion(event) || shouldCommitBackspaceDuringConversion(event)) {
        const HRESULT commitHr = DispatchOps::tryCommitOpenSession(*this, targetContext);
        if (SUCCEEDED(commitHr)) {
            *eaten = TRUE;
            return S_OK;
        }
    }

    const auto snapshot = engine.captureSessionSnapshot();
    const auto output = engine.processKeyEvent(event);
    *eaten = output.isConsumed ? TRUE : FALSE;
    if (*eaten == TRUE) {
        const bool dispatched = DispatchOps::tryDispatchOutput(
            *this,
            targetContext,
            output,
            outputMutatesDocument(output)
                ? std::optional<ime::engine::ImeEngine::SessionSnapshot>(snapshot)
                : std::nullopt,
            "dispatchEngineOutput failed for engine output");
        if (!dispatched && outputMutatesDocument(output)) {
            discardSessionAndCloseUi();
            *eaten = FALSE;
        }
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::OnKeyUp(
    ITfContext* context,
    WPARAM wParam,
    LPARAM lParam,
    BOOL* eaten) {
    (void)context;
    (void)wParam;
    (void)lParam;
    if (eaten == nullptr) {
        return E_POINTER;
    }

    *eaten = FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE TextService::OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) {
    (void)context;
    (void)guid;
    if (eaten == nullptr) {
        return E_POINTER;
    }

    *eaten = FALSE;
    return S_OK;
}

}
