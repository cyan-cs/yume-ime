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


#include "platform/tsf/idle_service.hpp"

#include "utils/logger.hpp"

namespace yume::platform::tsf {

namespace {

constexpr DWORD kIdleServiceIntervalMs = 1000;

}
IdleService::~IdleService() {
    stop();
}

bool IdleService::start(std::function<void()> tickHandler) {
    std::scoped_lock lock(mutex);
    if (running.load()) {
        return true;
    }

    stopEvent.reset(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!stopEvent) {
        YUME_LOG_ERROR("IdleService", "CreateEventW failed");
        return false;
    }

    onTick = std::move(tickHandler);
    running.store(true);
    workerThread.reset(::CreateThread(nullptr, 0, &IdleService::threadProc, this, 0, nullptr));
    if (!workerThread) {
        YUME_LOG_ERROR("IdleService", "CreateThread failed");
        running.store(false);
        onTick = {};
        stopEvent.reset();
        return false;
    }

    YUME_LOG_INFO("IdleService", "start");
    return true;
}

void IdleService::stop() {
    HANDLE thread = nullptr;
    HANDLE stopSignal = nullptr;
    {
        std::scoped_lock lock(mutex);
        if (!running.exchange(false)) {
            return;
        }
        thread = workerThread.get();
        stopSignal = stopEvent.get();
    }

    if (stopSignal != nullptr) {
        ::SetEvent(stopSignal);
    }

    if (thread != nullptr) {
        ::WaitForSingleObject(thread, INFINITE);
    }

    std::scoped_lock lock(mutex);
    workerThread.reset();
    stopEvent.reset();
    onTick = {};
    YUME_LOG_INFO("IdleService", "stop");
}

DWORD WINAPI IdleService::threadProc(LPVOID parameter) {
    static_cast<IdleService*>(parameter)->run();
    return 0;
}

void IdleService::run() {
    while (running.load()) {
        std::function<void()> tickHandler;
        HANDLE stopSignal = nullptr;
        {
            std::scoped_lock lock(mutex);
            tickHandler = onTick;
            stopSignal = stopEvent.get();
        }
        if (tickHandler) {
            tickHandler();
        }
        if (!running.load()) {
            break;
        }
        if (stopSignal == nullptr) {
            break;
        }
        if (::WaitForSingleObject(stopSignal, kIdleServiceIntervalMs) == WAIT_OBJECT_0) {
            break;
        }
    }
}

}
