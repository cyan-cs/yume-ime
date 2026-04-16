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

#include "platform/tsf/idle_service.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace yume::platform::tsf {

    class TextService;

    class ModuleState final {
    public:
        class SessionLease {
        public:
            SessionLease() = default;
            SessionLease(const SessionLease&) = delete;
            SessionLease& operator=(const SessionLease&) = delete;

            SessionLease(SessionLease&& other) noexcept;
            SessionLease& operator=(SessionLease&& other) noexcept;

            ~SessionLease();

            explicit operator bool() const { return state != nullptr; }

        private:
            friend class ModuleState;

            explicit SessionLease(ModuleState* owner) : state(owner) {}

            ModuleState* state = nullptr;
        };

        class ObjectLease {
        public:
            ObjectLease() = default;
            ObjectLease(const ObjectLease&) = delete;
            ObjectLease& operator=(const ObjectLease&) = delete;

            ObjectLease(ObjectLease&& other) noexcept;
            ObjectLease& operator=(ObjectLease&& other) noexcept;

            ~ObjectLease();

        private:
            friend class ModuleState;

            explicit ObjectLease(ModuleState* owner) : state(owner) {}

            ModuleState* state = nullptr;
        };

        static ModuleState& instance();

        std::optional<SessionLease> tryAcquireSession();
        uint32_t sessionCount() const;
        ObjectLease acquireObjectLease();
        uint32_t objectCount() const;
        void registerTextService(TextService* service);
        void unregisterTextService(TextService* service);

    private:
        ModuleState() = default;
        void releaseSessionLease();
        void releaseObjectLease();
        void serviceIdleEngines();

        mutable std::mutex mutex;
        IdleService idleService;
        std::vector<TextService*> registeredTextServices;
        uint32_t activeSessions = 0;
        uint32_t liveObjects = 0;
    };

}
