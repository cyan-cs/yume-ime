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

#include "ime/engine/ime_engine.hpp"
#include "platform/tsf/candidate_ui_element.hpp"
#include "platform/tsf/module_state.hpp"
#include "platform/tsf/text_service_config.hpp"
#include "ui/candidate_window/candidate_window.hpp"
#include "utils/com_object_base.hpp"

#include <msctf.h>
#include <windows.h>
#include <wrl/client.h>

#include <optional>
#include <vector>

namespace yume::platform::tsf {

    class TextEditSession;
    class TextServiceRegistrationTestAccessor;

    extern const CLSID kTextServiceClsid;

    class TextService final
        : public yume::utils::ComObjectBase<TextService>
        , public ITfTextInputProcessor
        , public ITfKeyEventSink
        , public ITfThreadMgrEventSink
        , public ITfCompositionSink
        , public ITfUIElementSink
        , public CandidateUiElementOwner {
    public:
        TextService();

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
        ULONG STDMETHODCALLTYPE AddRef() override;
        ULONG STDMETHODCALLTYPE Release() override;

        HRESULT STDMETHODCALLTYPE Activate(ITfThreadMgr* threadMgr, TfClientId clientId) override;
        HRESULT STDMETHODCALLTYPE Deactivate() override;

        HRESULT STDMETHODCALLTYPE OnSetFocus(BOOL foreground) override;
        HRESULT STDMETHODCALLTYPE OnTestKeyDown(
            ITfContext* context,
            WPARAM wParam,
            LPARAM lParam,
            BOOL* eaten) override;
        HRESULT STDMETHODCALLTYPE OnTestKeyUp(
            ITfContext* context,
            WPARAM wParam,
            LPARAM lParam,
            BOOL* eaten) override;
        HRESULT STDMETHODCALLTYPE OnKeyDown(
            ITfContext* context,
            WPARAM wParam,
            LPARAM lParam,
            BOOL* eaten) override;
        HRESULT STDMETHODCALLTYPE OnKeyUp(
            ITfContext* context,
            WPARAM wParam,
            LPARAM lParam,
            BOOL* eaten) override;
        HRESULT STDMETHODCALLTYPE OnPreservedKey(ITfContext* context, REFGUID guid, BOOL* eaten) override;

        HRESULT STDMETHODCALLTYPE OnInitDocumentMgr(ITfDocumentMgr* documentMgr) override;
        HRESULT STDMETHODCALLTYPE OnUninitDocumentMgr(ITfDocumentMgr* documentMgr) override;
        HRESULT STDMETHODCALLTYPE OnSetFocus(
            ITfDocumentMgr* documentMgrFocus,
            ITfDocumentMgr* documentMgrPrevFocus) override;
        HRESULT STDMETHODCALLTYPE OnPushContext(ITfContext* context) override;
        HRESULT STDMETHODCALLTYPE OnPopContext(ITfContext* context) override;

        HRESULT STDMETHODCALLTYPE OnCompositionTerminated(
            TfEditCookie editCookie,
            ITfComposition* composition) override;
        HRESULT STDMETHODCALLTYPE BeginUIElement(DWORD uiElementId, BOOL* show) override;
        HRESULT STDMETHODCALLTYPE UpdateUIElement(DWORD uiElementId) override;
        HRESULT STDMETHODCALLTYPE EndUIElement(DWORD uiElementId) override;

        bool onCandidateUiSelectionChanged(int32_t index) override;
        void onCandidateUiFinalizeRequested() override;
        void onCandidateUiAbortRequested() override;
        void serviceIdle();

    private:
        friend class TextEditSession;
        friend class DispatchOps;
        friend class EditOps;
        friend class TextServiceRegistrationTestAccessor;
        friend class yume::utils::ComObjectBase<TextService>;

        class DispatchOps;
        class EditOps;

        struct ContextSinkRegistration {
            Microsoft::WRL::ComPtr<ITfContext> context;
            Microsoft::WRL::ComPtr<ITfSource> source;
            DWORD cookie = TF_INVALID_COOKIE;

            ContextSinkRegistration() = default;
            ContextSinkRegistration(ContextSinkRegistration&& other) noexcept;
            ContextSinkRegistration& operator=(ContextSinkRegistration&& other) noexcept;
            ContextSinkRegistration(const ContextSinkRegistration&) = delete;
            ContextSinkRegistration& operator=(const ContextSinkRegistration&) = delete;
            ~ContextSinkRegistration();

            void reset();
        };

        struct UiElementRegistration {
            Microsoft::WRL::ComPtr<ITfUIElementMgr> manager;
            DWORD id = TF_INVALID_UIELEMENTID;

            UiElementRegistration() = default;
            UiElementRegistration(UiElementRegistration&& other) noexcept;
            UiElementRegistration& operator=(UiElementRegistration&& other) noexcept;
            UiElementRegistration(const UiElementRegistration&) = delete;
            UiElementRegistration& operator=(const UiElementRegistration&) = delete;
            ~UiElementRegistration();

            bool isActive() const { return id != TF_INVALID_UIELEMENTID; }
            void clear();
            void reset();
        };

        ~TextService();

        HRESULT adviseSinks();
        void unadviseSinks();
        HRESULT adviseCompositionSink(ITfContext* context);
        void unadviseCompositionSink(ITfContext* context);
        void clearCompositionSinks();
        HRESULT requestEditSession(ITfContext* context, const ime::engine::EngineOutput& output);
        HRESULT requestEditSession(
            ITfContext* context,
            const ime::engine::EngineOutput& output,
            bool requireSynchronous);
        HRESULT applyEngineOutput(
            TfEditCookie editCookie,
            ITfContext* context,
            const ime::engine::EngineOutput& output);
        HRESULT updateComposition(
            TfEditCookie editCookie,
            ITfContext* context,
            const ime::engine::CompositionState& compositionState);
        HRESULT commitText(
            TfEditCookie editCookie,
            ITfContext* context,
            std::u16string_view text);
        HRESULT clearComposition(TfEditCookie editCookie);
        void releaseFocusedContext();
        void releaseActiveComposition();
        void resetFocusTracking();
        void setFocusedContext(ITfContext* context);
        void setActiveComposition(ITfContext* context, ITfComposition* composition);
        ITfContext* resolveContext(ITfContext* preferredContext) const;
        Microsoft::WRL::ComPtr<ITfContext> getTopContext(ITfDocumentMgr* documentMgr) const;
        void updateFocusedDocumentManager(ITfDocumentMgr* documentMgr);
        void clearTrackedContext(ITfContext* context);
        HRESULT dispatchEngineOutput(ITfContext* context, const ime::engine::EngineOutput& output);
        HRESULT dispatchEngineOutput(
            ITfContext* context,
            const ime::engine::EngineOutput& output,
            bool requireSynchronous);
        void updateCandidateUi(ITfContext* context, const ime::engine::EngineOutput& output);
        ime::engine::EngineOutput adaptOutputForConfig(const ime::engine::EngineOutput& output) const;
        bool applyUiDrivenOutput(
            const std::optional<ime::engine::EngineOutput>& output,
            const ime::engine::ImeEngine::SessionSnapshot* snapshot = nullptr);
        void hideCandidateWindow();
        void resetCandidateUiState();
        std::optional<RECT> resolveCandidateAnchorRect(ITfContext* context) const;
        ITfDocumentMgr* resolveFocusedDocumentManager() const;
        HRESULT ensureCandidateUiElement(
            ITfContext* context,
            const ui::candidate_window::CandidateUIModel& model);
        void endCandidateUiElement();
        void syncCandidateWindowVisibilityFromUiElement();
        bool isCandidateUiShown() const;
        bool shouldConsumeKey(WPARAM wParam, LPARAM lParam) const;
        bool buildKeyEvent(WPARAM wParam, LPARAM lParam, ime::input::KeyEvent& event) const;
        void discardSessionAndCloseUi();
        bool shouldBypassTabNavigation(WPARAM wParam) const;
        bool shouldDiscardEmptyNonDirectSession(const ime::input::KeyEvent& event) const;
        ime::engine::EngineOutput buildDirectCommitOutput(const ime::input::KeyEvent& event) const;
        void tryRestartCompositionAfterCommit(
            ITfContext* targetContext,
            const ime::input::KeyEvent& event);
        bool shouldUseCapsLockAsImeToggle() const;
        bool shouldCommitConfiguredFullWidthDirectInput(const ime::input::KeyEvent& event) const;
        std::u16string buildConfiguredDirectCommitText(const ime::input::KeyEvent& event) const;
        bool shouldCommitPrintableDuringConversion(const ime::input::KeyEvent& event) const;
        bool shouldCommitEscapeDuringConversion(const ime::input::KeyEvent& event) const;
        bool shouldCommitBackspaceDuringConversion(const ime::input::KeyEvent& event) const;
        void handleFocusLoss(ITfContext* context);

        Microsoft::WRL::ComPtr<ITfThreadMgr> threadManager;
        TfClientId clientId = TF_CLIENTID_NULL;
        bool sessionActive = false;
        DWORD threadMgrEventSinkCookie = TF_INVALID_COOKIE;
        DWORD uiElementSinkCookie = TF_INVALID_COOKIE;
        bool keyEventSinkAdvised = false;
        std::vector<ContextSinkRegistration> compositionSinkRegistrations;
        Microsoft::WRL::ComPtr<ITfDocumentMgr> focusedDocumentManager;
        Microsoft::WRL::ComPtr<ITfContext> focusedContext;
        Microsoft::WRL::ComPtr<ITfContext> activeCompositionContext;
        Microsoft::WRL::ComPtr<ITfComposition> activeComposition;
        RECT lastKnownAnchorRect{32, 32, 272, 56};
        bool hasLastKnownAnchorRect = false;
        Microsoft::WRL::ComPtr<CandidateUiElement> candidateUiElement;
        UiElementRegistration candidateUiRegistration;
        TextServiceConfig config;
        ime::engine::ImeEngine engine;
        ui::candidate_window::CandidateWindow candidateWindow;
        ModuleState::ObjectLease objectLease;
        std::optional<ModuleState::SessionLease> sessionLease;
    };

}
