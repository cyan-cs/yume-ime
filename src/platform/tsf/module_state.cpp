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



#include "platform/tsf/module_state.hpp"
#include "platform/tsf/text_service.hpp"
#include "utils/logger.hpp"

#include <algorithm>
#include <wrl/client.h>

namespace yume::platform::tsf {

ModuleState::SessionLease::SessionLease(SessionLease&& other) noexcept
    : state(other.state) {
    other.state = nullptr;
}

ModuleState::SessionLease& ModuleState::SessionLease::operator=(SessionLease&& other) noexcept {
    if (this != &other) {
        if (state != nullptr) {
            state->releaseSessionLease();
        }
        state = other.state;
        other.state = nullptr;
    }
    return *this;
}

ModuleState::SessionLease::~SessionLease() {
    if (state != nullptr) {
        state->releaseSessionLease();
    }
}

ModuleState::ObjectLease::ObjectLease(ObjectLease&& other) noexcept
    : state(other.state) {
    other.state = nullptr;
}

ModuleState::ObjectLease& ModuleState::ObjectLease::operator=(ObjectLease&& other) noexcept {
    if (this != &other) {
        if (state != nullptr) {
            state->releaseObjectLease();
        }
        state = other.state;
        other.state = nullptr;
    }
    return *this;
}

ModuleState::ObjectLease::~ObjectLease() {
    if (state != nullptr) {
        state->releaseObjectLease();
    }
}

ModuleState& ModuleState::instance() {
    static ModuleState state;
    return state;
}

std::optional<ModuleState::SessionLease> ModuleState::tryAcquireSession() {
    std::scoped_lock lock(mutex);
    if (activeSessions == 0 && !idleService.start([this]() { serviceIdleEngines(); })) {
        YUME_LOG_ERROR("ModuleState", "failed to start idleService");
        return std::nullopt;
    }
    ++activeSessions;
    YUME_LOG_INFO("ModuleState", "acquireSession activeSessions=", activeSessions);
    return SessionLease(this);
}

void ModuleState::releaseSessionLease() {
    bool shouldStop = false;
    {
        std::scoped_lock lock(mutex);
        if (activeSessions == 0) {
            return;
        }

        if (--activeSessions == 0) {
            shouldStop = true;
        }
    }

    if (shouldStop) {
        idleService.stop();
    }
    YUME_LOG_INFO("ModuleState", "releaseSession activeSessions=", sessionCount());
}

uint32_t ModuleState::sessionCount() const {
    std::scoped_lock lock(mutex);
    return activeSessions;
}

ModuleState::ObjectLease ModuleState::acquireObjectLease() {
    std::scoped_lock lock(mutex);
    ++liveObjects;
    YUME_LOG_TRACE("ModuleState", "acquireObject liveObjects=", liveObjects);
    return ObjectLease(this);
}

void ModuleState::releaseObjectLease() {
    std::scoped_lock lock(mutex);
    if (liveObjects == 0) {
        return;
    }
    --liveObjects;
    YUME_LOG_TRACE("ModuleState", "releaseObject liveObjects=", liveObjects);
}

uint32_t ModuleState::objectCount() const {
    std::scoped_lock lock(mutex);
    return liveObjects;
}

void ModuleState::registerTextService(TextService* service) {
    if (service == nullptr) {
        return;
    }

    std::scoped_lock lock(mutex);
    const auto it = std::find(registeredTextServices.begin(), registeredTextServices.end(), service);
    if (it == registeredTextServices.end()) {
        registeredTextServices.push_back(service);
    }
}

void ModuleState::unregisterTextService(TextService* service) {
    if (service == nullptr) {
        return;
    }

    std::scoped_lock lock(mutex);
    registeredTextServices.erase(
        std::remove(registeredTextServices.begin(), registeredTextServices.end(), service),
        registeredTextServices.end());
}

void ModuleState::serviceIdleEngines() {
    std::vector<Microsoft::WRL::ComPtr<TextService>> services;
    {
        std::scoped_lock lock(mutex);
        services.reserve(registeredTextServices.size());
        for (auto* service : registeredTextServices) {
            if (service != nullptr) {
                Microsoft::WRL::ComPtr<TextService> retainedService;
                retainedService = service;
                services.push_back(std::move(retainedService));
            }
        }
    }

    for (const auto& service : services) {
        if (service != nullptr) {
            service->serviceIdle();
        }
    }
}

}
