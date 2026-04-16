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


#include "platform/tsf/class_factory.hpp"
#include "platform/tsf/module_state.hpp"
#include "platform/tsf/registration_helpers.hpp"
#include "platform/tsf/text_service.hpp"
#include "utils/com_ptr.hpp"
#include "utils/logger.hpp"

#include <windows.h>

#include <string>

namespace {

HMODULE g_moduleHandle = nullptr;

}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;

    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            g_moduleHandle = hModule;
            DisableThreadLibraryCalls(hModule);
            break;
        case DLL_PROCESS_DETACH:
            if (g_moduleHandle == hModule) {
                g_moduleHandle = nullptr;
            }
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}

extern "C" STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (ppv == nullptr) {
        return E_POINTER;
    }

    *ppv = nullptr;
    if (rclsid != yume::platform::tsf::kTextServiceClsid) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto factory = yume::utils::makeComPtr<yume::platform::tsf::ClassFactory>();
    if (factory == nullptr) {
        return E_OUTOFMEMORY;
    }

    return factory->QueryInterface(riid, ppv);
}

extern "C" STDAPI DllCanUnloadNow() {
    return (yume::platform::tsf::ClassFactory::serverLockCount() == 0 &&
            yume::platform::tsf::ModuleState::instance().sessionCount() == 0 &&
            yume::platform::tsf::ModuleState::instance().objectCount() == 0)
        ? S_OK
        : S_FALSE;
}

extern "C" STDAPI DllRegisterServer() {
    const std::wstring modulePath = yume::platform::tsf::detail::getModulePath(g_moduleHandle);
    if (modulePath.empty()) {
        YUME_LOG_ERROR("DllRegisterServer", "getModulePath returned empty path");
        return E_FAIL;
    }

    return yume::platform::tsf::detail::withComInitialized([&]() -> HRESULT {
        return yume::platform::tsf::detail::registerServer(modulePath);
    });
}

extern "C" STDAPI DllUnregisterServer() {
    return yume::platform::tsf::detail::withComInitialized([]() -> HRESULT {
        return yume::platform::tsf::detail::unregisterServer();
    });
}
