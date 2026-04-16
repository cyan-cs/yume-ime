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



#include "platform/tsf/text_service.hpp"
#include "platform/tsf/tsf_helpers.hpp"

#include "utils/logger.hpp"

#include <algorithm>

namespace yume::platform::tsf {

TextService::ContextSinkRegistration::ContextSinkRegistration(ContextSinkRegistration&& other) noexcept
    : context(std::move(other.context))
    , source(std::move(other.source))
    , cookie(other.cookie) {
    other.cookie = TF_INVALID_COOKIE;
}

TextService::ContextSinkRegistration& TextService::ContextSinkRegistration::operator=(
    ContextSinkRegistration&& other) noexcept {
    if (this != &other) {
        reset();
        context = std::move(other.context);
        source = std::move(other.source);
        cookie = other.cookie;
        other.cookie = TF_INVALID_COOKIE;
    }
    return *this;
}

TextService::ContextSinkRegistration::~ContextSinkRegistration() {
    reset();
}

void TextService::ContextSinkRegistration::reset() {
    helpers::unadviseSink(source.Get(), &cookie);
    source.Reset();
    context.Reset();
}

HRESULT TextService::adviseSinks() {
    Microsoft::WRL::ComPtr<ITfKeystrokeMgr> keystrokeMgr;
    HRESULT hr = helpers::queryInterface(threadManager.Get(), keystrokeMgr);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "QueryInterface(ITfKeystrokeMgr)", hr);
        return hr;
    }

    hr = keystrokeMgr->AdviseKeyEventSink(clientId, static_cast<ITfKeyEventSink*>(this), TRUE);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "AdviseKeyEventSink", hr);
        return hr;
    }
    keyEventSinkAdvised = true;

    Microsoft::WRL::ComPtr<ITfSource> source;
    hr = helpers::queryInterface(threadManager.Get(), source);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "QueryInterface(ITfSource)", hr);
        unadviseSinks();
        return hr;
    }

    hr = helpers::adviseSink(
        source.Get(),
        IID_ITfThreadMgrEventSink,
        static_cast<ITfThreadMgrEventSink*>(this),
        &threadMgrEventSinkCookie);
    if (SUCCEEDED(hr)) {
        hr = helpers::adviseSink(
            source.Get(),
            IID_ITfUIElementSink,
            static_cast<ITfUIElementSink*>(this),
            &uiElementSinkCookie);
    }
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "AdviseSink", hr);
        unadviseSinks();
        return hr;
    }

    YUME_LOG_INFO("TextService", "adviseSinks complete");
    return S_OK;
}

void TextService::unadviseSinks() {
    clearCompositionSinks();

    if (threadManager != nullptr &&
        (threadMgrEventSinkCookie != TF_INVALID_COOKIE || uiElementSinkCookie != TF_INVALID_COOKIE)) {
        Microsoft::WRL::ComPtr<ITfSource> source;
        if (SUCCEEDED(helpers::queryInterface(threadManager.Get(), source))) {
            helpers::unadviseSink(source.Get(), &threadMgrEventSinkCookie);
            helpers::unadviseSink(source.Get(), &uiElementSinkCookie);
        }
    }
    threadMgrEventSinkCookie = TF_INVALID_COOKIE;
    uiElementSinkCookie = TF_INVALID_COOKIE;

    if (threadManager != nullptr && keyEventSinkAdvised && clientId != TF_CLIENTID_NULL) {
        Microsoft::WRL::ComPtr<ITfKeystrokeMgr> keystrokeMgr;
        if (SUCCEEDED(helpers::queryInterface(threadManager.Get(), keystrokeMgr))) {
            keystrokeMgr->UnadviseKeyEventSink(clientId);
        }
    }
    keyEventSinkAdvised = false;
}

HRESULT TextService::adviseCompositionSink(ITfContext* context) {
    if (context == nullptr) {
        return E_INVALIDARG;
    }

    const auto existing = std::find_if(
        compositionSinkRegistrations.begin(),
        compositionSinkRegistrations.end(),
        [context](const ContextSinkRegistration& registration) {
            return registration.context.Get() == context;
        });
    if (existing != compositionSinkRegistrations.end()) {
        return S_OK;
    }

    Microsoft::WRL::ComPtr<ITfSource> source;
    const HRESULT hr = helpers::queryInterface(context, source);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "Context QueryInterface(ITfSource)", hr);
        return hr;
    }

    DWORD cookie = TF_INVALID_COOKIE;
    const HRESULT adviseHr = helpers::adviseSink(
        source.Get(),
        IID_ITfCompositionSink,
        static_cast<ITfCompositionSink*>(this),
        &cookie);
    if (FAILED(adviseHr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "TextService", "AdviseCompositionSink", adviseHr);
        return adviseHr;
    }

    ContextSinkRegistration registration;
    registration.context = context;
    registration.source = source;
    registration.cookie = cookie;
    compositionSinkRegistrations.push_back(std::move(registration));
    YUME_LOG_DEBUG("TextService", "adviseCompositionSink count=", compositionSinkRegistrations.size());
    return S_OK;
}

void TextService::unadviseCompositionSink(ITfContext* context) {
    const auto it = std::find_if(
        compositionSinkRegistrations.begin(),
        compositionSinkRegistrations.end(),
        [context](const ContextSinkRegistration& registration) {
            return registration.context.Get() == context;
        });
    if (it == compositionSinkRegistrations.end()) {
        return;
    }

    compositionSinkRegistrations.erase(it);
}

void TextService::clearCompositionSinks() {
    compositionSinkRegistrations.clear();
}

}
