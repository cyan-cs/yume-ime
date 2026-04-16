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


#include "platform/tsf/registration_helpers.hpp"

#include "platform/tsf/text_service.hpp"
#include "utils/logger.hpp"
#include "utils/win_raii.hpp"

#include <msctf.h>
#include <wrl/client.h>

#include <array>
#include <vector>

namespace yume::platform::tsf::detail {

namespace {

constexpr wchar_t kImeName[] = L"yume-ime";
constexpr LANGID kImeLangId = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);

const GUID kLanguageProfileGuid =
    {0x7e7a20f1, 0x1ab4, 0x4f09, {0x84, 0x10, 0x5a, 0x0a, 0x3c, 0x69, 0x22, 0x91}};

constexpr DWORD kRegistryValueWriteAccess = KEY_SET_VALUE;
constexpr DWORD kRegistryValueType = REG_SZ;
constexpr ULONG kRegisterProfileIconIndex = 0;
constexpr DWORD kModulePathInitialBufferSize = MAX_PATH;

std::wstring guidToString(REFGUID guid) {
    std::array<wchar_t, 39> text{};
    const int length = StringFromGUID2(guid, text.data(), static_cast<int>(text.size()));
    return (length > 0) ? std::wstring(text.data()) : std::wstring{};
}

HRESULT setRegistryString(HKEY root, const std::wstring& subKey, const wchar_t* valueName, const std::wstring& value) {
    yume::utils::UniqueRegistryKey key;
    HKEY rawKey = nullptr;
    const LONG createStatus = RegCreateKeyExW(
        root,
        subKey.c_str(),
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        kRegistryValueWriteAccess,
        nullptr,
        &rawKey,
        nullptr);
    if (createStatus != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(createStatus);
    }
    key.reset(rawKey);

    const DWORD byteCount = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const LONG setStatus = RegSetValueExW(
        key.get(),
        valueName,
        0,
        kRegistryValueType,
        reinterpret_cast<const BYTE*>(value.c_str()),
        byteCount);
    return (setStatus == ERROR_SUCCESS) ? S_OK : HRESULT_FROM_WIN32(setStatus);
}

HRESULT deleteRegistryTreeIfPresent(HKEY root, const std::wstring& subKey) {
    const LONG status = RegDeleteTreeW(root, subKey.c_str());
    if (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
        return S_OK;
    }
    return HRESULT_FROM_WIN32(status);
}

HRESULT registerComServer(const std::wstring& modulePath) {
    const std::wstring clsidText = guidToString(kTextServiceClsid);
    if (clsidText.empty()) {
        YUME_LOG_ERROR("DllRegisterServer", "guidToString returned empty CLSID text");
        return E_FAIL;
    }

    const std::wstring clsidKey = L"Software\\Classes\\CLSID\\" + clsidText;
    HRESULT hr = setRegistryString(HKEY_CURRENT_USER, clsidKey, nullptr, kImeName);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "DllRegisterServer", "setRegistryString(CLSID)", hr);
        return hr;
    }

    hr = setRegistryString(HKEY_CURRENT_USER, clsidKey + L"\\InprocServer32", nullptr, modulePath);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(
            yume::utils::Logger::Level::Error,
            "DllRegisterServer",
            "setRegistryString(InprocServer32 default)",
            hr);
        return hr;
    }

    hr = setRegistryString(HKEY_CURRENT_USER, clsidKey + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(
            yume::utils::Logger::Level::Error,
            "DllRegisterServer",
            "setRegistryString(InprocServer32 ThreadingModel)",
            hr);
        return hr;
    }

    YUME_LOG_INFO("DllRegisterServer", "registerComServer complete");
    return S_OK;
}

HRESULT unregisterComServer() {
    const std::wstring clsidText = guidToString(kTextServiceClsid);
    if (clsidText.empty()) {
        YUME_LOG_ERROR("DllRegisterServer", "guidToString returned empty CLSID text during unregister");
        return E_FAIL;
    }

    const HRESULT hr = deleteRegistryTreeIfPresent(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\" + clsidText);
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "DllRegisterServer", "deleteRegistryTreeIfPresent(CLSID)", hr);
    }
    return hr;
}

HRESULT registerLanguageProfile(const std::wstring& modulePath) {
    Microsoft::WRL::ComPtr<ITfInputProcessorProfileMgr> profileMgr;
    HRESULT hr = CoCreateInstance(
        CLSID_TF_InputProcessorProfiles,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfileMgr,
        reinterpret_cast<void**>(profileMgr.GetAddressOf()));
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(
            yume::utils::Logger::Level::Error,
            "DllRegisterServer",
            "CoCreateInstance(ITfInputProcessorProfileMgr)",
            hr);
        return hr;
    }

    hr = profileMgr->RegisterProfile(
        kTextServiceClsid,
        kImeLangId,
        kLanguageProfileGuid,
        kImeName,
        static_cast<ULONG>(wcslen(kImeName)),
        modulePath.c_str(),
        static_cast<ULONG>(modulePath.size()),
        kRegisterProfileIconIndex,
        nullptr,
        0,
        TRUE,
        0);
    if (SUCCEEDED(hr)) {
        const HRESULT activateHr = profileMgr->ActivateProfile(
            TF_PROFILETYPE_INPUTPROCESSOR,
            kImeLangId,
            kTextServiceClsid,
            kLanguageProfileGuid,
            nullptr,
            TF_IPPMF_FORSESSION | TF_IPPMF_ENABLEPROFILE | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
        if (FAILED(activateHr)) {
            YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "DllRegisterServer", "ActivateProfile", activateHr);
        }
    } else {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "DllRegisterServer", "RegisterProfile", hr);
    }

    if (SUCCEEDED(hr)) {
        YUME_LOG_INFO("DllRegisterServer", "registerLanguageProfile complete");
    }
    return hr;
}

HRESULT unregisterLanguageProfile() {
    Microsoft::WRL::ComPtr<ITfInputProcessorProfileMgr> profileMgr;
    HRESULT hr = CoCreateInstance(
        CLSID_TF_InputProcessorProfiles,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfileMgr,
        reinterpret_cast<void**>(profileMgr.GetAddressOf()));
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(
            yume::utils::Logger::Level::Error,
            "DllRegisterServer",
            "CoCreateInstance(ITfInputProcessorProfileMgr) during unregister",
            hr);
        return hr;
    }

    HRESULT result = profileMgr->DeactivateProfile(
        TF_PROFILETYPE_INPUTPROCESSOR,
        kImeLangId,
        kTextServiceClsid,
        kLanguageProfileGuid,
        nullptr,
        TF_IPPMF_FORSESSION);
    if (FAILED(result)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Warn, "DllRegisterServer", "DeactivateProfile", result);
        result = S_OK;
    }

    hr = profileMgr->UnregisterProfile(kTextServiceClsid, kImeLangId, kLanguageProfileGuid, 0);
    if (FAILED(hr) && SUCCEEDED(result)) {
        result = hr;
    }
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Warn, "DllRegisterServer", "UnregisterProfile", hr);
    }

    hr = profileMgr->ReleaseInputProcessor(kTextServiceClsid, 0);
    if (FAILED(hr) && SUCCEEDED(result)) {
        result = hr;
    }
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Warn, "DllRegisterServer", "ReleaseInputProcessor", hr);
    }

    return result;
}

HRESULT registerCategories() {
    Microsoft::WRL::ComPtr<ITfCategoryMgr> categoryMgr;
    HRESULT hr = CoCreateInstance(
        CLSID_TF_CategoryMgr,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr,
        reinterpret_cast<void**>(categoryMgr.GetAddressOf()));
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "DllRegisterServer", "CoCreateInstance(ITfCategoryMgr)", hr);
        return hr;
    }

    const GUID* categories[] = {
        &GUID_TFCAT_TIP_KEYBOARD,
        &GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
        &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    };

    for (const GUID* category : categories) {
        hr = categoryMgr->RegisterCategory(kTextServiceClsid, *category, kTextServiceClsid);
        if (FAILED(hr)) {
            YUME_LOG_HRESULT(yume::utils::Logger::Level::Error, "DllRegisterServer", "RegisterCategory", hr);
            return hr;
        }
    }

    YUME_LOG_INFO("DllRegisterServer", "registerCategories complete");
    return S_OK;
}

HRESULT unregisterCategories() {
    Microsoft::WRL::ComPtr<ITfCategoryMgr> categoryMgr;
    HRESULT hr = CoCreateInstance(
        CLSID_TF_CategoryMgr,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr,
        reinterpret_cast<void**>(categoryMgr.GetAddressOf()));
    if (FAILED(hr)) {
        YUME_LOG_HRESULT(
            yume::utils::Logger::Level::Error,
            "DllRegisterServer",
            "CoCreateInstance(ITfCategoryMgr) during unregister",
            hr);
        return hr;
    }

    HRESULT result = S_OK;
    const GUID* categories[] = {
        &GUID_TFCAT_TIP_KEYBOARD,
        &GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
        &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    };

    for (const GUID* category : categories) {
        hr = categoryMgr->UnregisterCategory(kTextServiceClsid, *category, kTextServiceClsid);
        if (FAILED(hr) && SUCCEEDED(result)) {
            result = hr;
        }
        if (FAILED(hr)) {
            YUME_LOG_HRESULT(yume::utils::Logger::Level::Warn, "DllRegisterServer", "UnregisterCategory", hr);
        }
    }

    return result;
}

}

std::wstring getModulePath(HMODULE moduleHandle) {
    if (moduleHandle == nullptr) {
        return {};
    }

    std::vector<wchar_t> buffer(kModulePathInitialBufferSize);
    while (true) {
        const DWORD copied = GetModuleFileNameW(moduleHandle, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (copied == 0) {
            return {};
        }
        if (copied < buffer.size() - 1) {
            return std::wstring(buffer.data(), copied);
        }
    }
}

HRESULT registerServer(const std::wstring& modulePath) {
    YUME_LOG_INFO("DllRegisterServer", "begin");
    HRESULT hr = registerComServer(modulePath);
    if (FAILED(hr)) {
        return hr;
    }

    hr = registerLanguageProfile(modulePath);
    if (FAILED(hr)) {
        unregisterLanguageProfile();
        unregisterComServer();
        return hr;
    }

    hr = registerCategories();
    if (FAILED(hr)) {
        unregisterCategories();
        unregisterLanguageProfile();
        unregisterComServer();
        return hr;
    }

    YUME_LOG_INFO("DllRegisterServer", "complete");
    return S_OK;
}

HRESULT unregisterServer() {
    YUME_LOG_INFO("DllRegisterServer", "begin unregister");
    HRESULT result = unregisterCategories();

    const HRESULT profileHr = unregisterLanguageProfile();
    if (FAILED(profileHr) && SUCCEEDED(result)) {
        result = profileHr;
    }

    const HRESULT comHr = unregisterComServer();
    if (FAILED(comHr) && SUCCEEDED(result)) {
        result = comHr;
    }

    if (SUCCEEDED(result)) {
        YUME_LOG_INFO("DllRegisterServer", "unregister complete");
    }
    return result;
}

HRESULT withComInitialized(const std::function<HRESULT()>& callback) {
    yume::utils::ScopedComApartment comApartment;
    const HRESULT initHr = comApartment.status();
    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE) {
        return initHr;
    }

    return callback();
}

}
