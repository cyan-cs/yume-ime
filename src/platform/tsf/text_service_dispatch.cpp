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

#include "utils/logger.hpp"

namespace yume::platform::tsf {

void TextService::DispatchOps::closeCandidateUi(TextService& service) {
    service.endCandidateUiElement();
    service.hideCandidateWindow();
}

void TextService::DispatchOps::handleDispatchFailure(TextService& service, const char* failureLabel) {
    YUME_LOG_ERROR("TextService", failureLabel);
    closeCandidateUi(service);
}

void TextService::DispatchOps::handleDispatchFailure(
    TextService& service,
    const ime::engine::ImeEngine::SessionSnapshot& snapshot,
    const char* failureLabel) {
    service.engine.restoreSessionSnapshot(snapshot);
    handleDispatchFailure(service, failureLabel);
}

void TextService::DispatchOps::handleDispatchFailure(
    TextService& service,
    const std::optional<ime::engine::ImeEngine::SessionSnapshot>& snapshot,
    const char* failureLabel) {
    if (snapshot.has_value()) {
        service.engine.restoreSessionSnapshot(*snapshot);
    }
    handleDispatchFailure(service, failureLabel);
}

bool TextService::DispatchOps::tryDispatchOutput(
    TextService& service,
    ITfContext* context,
    const ime::engine::EngineOutput& output,
    const std::optional<ime::engine::ImeEngine::SessionSnapshot>& rollbackSnapshot,
    const char* failureLabel,
    bool closeOnlyWhenContextMissing) {
    if (context == nullptr) {
        if (closeOnlyWhenContextMissing) {
            closeCandidateUi(service);
        } else {
            handleDispatchFailure(service, rollbackSnapshot, failureLabel);
        }
        return false;
    }

    if (FAILED(service.dispatchEngineOutput(context, output))) {
        handleDispatchFailure(service, rollbackSnapshot, failureLabel);
        return false;
    }

    return true;
}

HRESULT TextService::DispatchOps::dispatchOutputWithRollback(
    TextService& service,
    ITfContext* context,
    const ime::engine::EngineOutput& output,
    const ime::engine::ImeEngine::SessionSnapshot& rollbackSnapshot,
    const char* failureLabel) {
    if (context == nullptr) {
        return E_INVALIDARG;
    }

    const HRESULT hr = service.dispatchEngineOutput(context, output);
    if (FAILED(hr)) {
        service.engine.restoreSessionSnapshot(rollbackSnapshot);
        YUME_LOG_ERROR("TextService", failureLabel);
    }
    return hr;
}

void TextService::DispatchOps::dispatchOutputOrCloseCandidateUi(
    TextService& service,
    ITfContext* context,
    const ime::engine::EngineOutput& output,
    const char* failureLabel) {
    tryDispatchOutput(service, context, output, std::nullopt, failureLabel, true);
}

HRESULT TextService::DispatchOps::tryCommitOpenSession(TextService& service, ITfContext* context) {
    if (context == nullptr) {
        return E_INVALIDARG;
    }

    switch (service.engine.getCurrentState()) {
        case ime::state::ImeState::Composing: {
            const auto snapshot = service.engine.captureSessionSnapshot();
            const auto output = service.engine.processKeyEvent(
                ime::input::KeyEvent(ime::input::KeyCode::Enter, std::nullopt, true, false, false, false));
            if (!output.isConsumed) {
                return S_FALSE;
            }
            return dispatchOutputWithRollback(
                service,
                context,
                output,
                snapshot,
                "dispatchEngineOutput failed while committing composing session");
        }
        case ime::state::ImeState::Converting: {
            const auto snapshot = service.engine.captureSessionSnapshot();
            const auto output = service.engine.finalizeCandidateSelection();
            if (!output.has_value()) {
                return S_FALSE;
            }
            return dispatchOutputWithRollback(
                service,
                context,
                *output,
                snapshot,
                "dispatchEngineOutput failed while committing converting session");
        }
        default:
            return S_FALSE;
    }
}

}
