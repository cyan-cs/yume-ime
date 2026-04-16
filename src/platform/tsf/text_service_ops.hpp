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

#include "platform/tsf/text_service.hpp"

namespace yume::platform::tsf {

class TextService::DispatchOps {
public:
    static void closeCandidateUi(TextService& service);
    static void handleDispatchFailure(TextService& service, const char* failureLabel);
    static void handleDispatchFailure(
        TextService& service,
        const ime::engine::ImeEngine::SessionSnapshot& snapshot,
        const char* failureLabel);
    static void handleDispatchFailure(
        TextService& service,
        const std::optional<ime::engine::ImeEngine::SessionSnapshot>& snapshot,
        const char* failureLabel);
    static bool tryDispatchOutput(
        TextService& service,
        ITfContext* context,
        const ime::engine::EngineOutput& output,
        const std::optional<ime::engine::ImeEngine::SessionSnapshot>& rollbackSnapshot,
        const char* failureLabel,
        bool closeOnlyWhenContextMissing = false);
    static HRESULT dispatchOutputWithRollback(
        TextService& service,
        ITfContext* context,
        const ime::engine::EngineOutput& output,
        const ime::engine::ImeEngine::SessionSnapshot& rollbackSnapshot,
        const char* failureLabel);
    static void dispatchOutputOrCloseCandidateUi(
        TextService& service,
        ITfContext* context,
        const ime::engine::EngineOutput& output,
        const char* failureLabel);
    static HRESULT tryCommitOpenSession(TextService& service, ITfContext* context);
};

class TextService::EditOps {
public:
    static HRESULT logEditSessionResult(const TextService& service, const char* action, HRESULT hr);
    static HRESULT requestReadWriteEditSession(
        const TextService& service,
        ITfContext* context,
        ITfEditSession* editSession,
        bool requireSynchronous);
    static HRESULT getActiveCompositionRange(const TextService& service, ITfRange** range);
    static HRESULT ensureActiveComposition(
        TextService& service,
        TfEditCookie editCookie,
        ITfContext* context,
        ITfRange** range);
    static void updateLastKnownAnchorRect(
        TextService& service,
        TfEditCookie editCookie,
        ITfContext* context,
        ITfRange* range);
    static HRESULT insertTextAtSelection(
        const TextService& service,
        TfEditCookie editCookie,
        ITfContext* context,
        std::u16string_view text,
        ITfRange** insertedRange);
};

}
