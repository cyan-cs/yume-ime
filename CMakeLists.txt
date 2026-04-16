cmake_minimum_required(VERSION 3.15)
project(YumeIME VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure common compile options
if (MSVC)
    add_compile_options(/W4 /WX /EHsc /utf-8)
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " /O2 /GL")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /LTCG")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS_RELEASE " /LTCG")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /LTCG")
else()
    add_compile_options(-Wall -Wextra -Werror)
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -O3 -march=native -flto")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " -flto")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS_RELEASE " -flto")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " -flto")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Core Engine Static Library (Testable purely in C++)
set(CORE_ENGINE_SOURCES 
    "src/ime/composition/buffer.cpp"
    "src/ime/composition/converter.cpp"
    "src/ime/composition/normalizer.cpp"
    "src/ime/composition/romaji_table.cpp"
    "src/ime/dictionary/black_db.cpp"
    "src/ime/dictionary/default_db.cpp"
    "src/ime/dictionary/default_db_helpers.cpp"
    "src/ime/dictionary/default_db_loader.cpp"
    "src/ime/dictionary/default_db_trie.cpp"
    "src/ime/dictionary/dictionary.cpp"
    "src/ime/dictionary/dictionary_candidates.cpp"
    "src/ime/dictionary/dictionary_internal.cpp"
    "src/ime/dictionary/dictionary_persistence.cpp"
    "src/ime/dictionary/dictionary_reload.cpp"
    "src/ime/dictionary/user_db.cpp"
    "src/ime/engine/ime_engine.cpp"
    "src/ime/engine/ime_engine_input.cpp"
    "src/ime/engine/ime_engine_segments.cpp"
    "src/utils/app_paths.cpp"
    "src/utils/logger.cpp"
    "src/utils/windows_theme.cpp"
)
add_library(ImeCore STATIC ${CORE_ENGINE_SOURCES})
target_include_directories(ImeCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# IME DLL (Platform / TSF Adapter + UI)
set(TSF_ADAPTER_SOURCES 
    "src/platform/tsf/candidate_ui_element.cpp"
    "src/platform/tsf/class_factory.cpp"
    "src/platform/tsf/DllMain.cpp"
    "src/platform/tsf/idle_service.cpp"
    "src/platform/tsf/module_state.cpp"
    "src/platform/tsf/registration_helpers.cpp"
    "src/platform/tsf/text_service.cpp"
    "src/platform/tsf/text_service_activation.cpp"
    "src/platform/tsf/text_service_candidate_ui.cpp"
    "src/platform/tsf/text_service_candidate_window.cpp"
    "src/platform/tsf/text_service_commit.cpp"
    "src/platform/tsf/text_service_composition.cpp"
    "src/platform/tsf/text_service_config.cpp"
    "src/platform/tsf/text_service_dispatch.cpp"
    "src/platform/tsf/text_service_edit_session.cpp"
    "src/platform/tsf/text_service_focus.cpp"
    "src/platform/tsf/text_service_input.cpp"
    "src/platform/tsf/text_service_keymap.cpp"
    "src/platform/tsf/text_service_keymap_internal.cpp"
    "src/platform/tsf/text_service_output.cpp"
    "src/platform/tsf/text_service_sinks.cpp"
    "src/platform/tsf/text_service_state.cpp"
    "src/platform/tsf/text_service_ui_element.cpp"
    "src/platform/tsf/tsf_helpers.cpp"
)
set(UI_SOURCES 
    "src/ui/candidate_window/candidate_window.cpp"
    "src/ui/candidate_window/candidate_window_theme.cpp"
)

# Define the DLL target
add_library(YumeIME SHARED ${TSF_ADAPTER_SOURCES} ${UI_SOURCES})
target_link_options(YumeIME PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/platform/tsf/yume.def")
# Link the Core Engine and necessary Windows libraries
target_link_libraries(YumeIME PRIVATE ImeCore d2d1 dwrite windowscodecs uxtheme imm32 advapi32 ole32 oleaut32 uuid)
target_link_libraries(YumeIME PRIVATE user32 gdi32)

function(yume_copy_runtime_data target_name)
    yume_copy_runtime_data_from(
        ${target_name}
        "${CMAKE_CURRENT_SOURCE_DIR}/data/dictionary/dictionary"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/dictionary/db")
endfunction()

function(yume_copy_runtime_data_from target_name dictionary_source_dir db_source_dir)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/data/dictionary/db"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${target_name}>/data/dictionary/dictionary"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/data/dictionary/dictionary"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/data/settings"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${dictionary_source_dir}"
            "$<TARGET_FILE_DIR:${target_name}>/data/dictionary/dictionary"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${db_source_dir}/user_db.bin"
            "$<TARGET_FILE_DIR:${target_name}>/data/dictionary/db/user_db.bin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${db_source_dir}/black_db.bin"
            "$<TARGET_FILE_DIR:${target_name}>/data/dictionary/db/black_db.bin"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/data/settings/text_service_config.json"
            "$<TARGET_FILE_DIR:${target_name}>/data/settings/text_service_config.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/data/settings/logging_config.json"
            "$<TARGET_FILE_DIR:${target_name}>/data/settings/logging_config.json")
endfunction()

yume_copy_runtime_data(YumeIME)

option(YUME_BUILD_TESTS "Build unit tests" OFF)
option(YUME_BUILD_SELFTESTS "Build offline self-test executable" ON)
option(YUME_BUILD_BENCHMARKS "Build benchmark executable" ON)
option(YUME_USE_BUNDLED_GTEST_COMPAT "Use bundled offline gtest-compatible test framework" ON)

if (YUME_BUILD_TESTS)
    if (YUME_USE_BUNDLED_GTEST_COMPAT)
        add_library(gtest INTERFACE)
        target_include_directories(gtest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/tests/support)

        add_library(gtest_main STATIC
            "tests/support/gtest_main.cpp"
        )
        target_include_directories(gtest_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests/support)
        target_link_libraries(gtest_main PUBLIC gtest)
    else()
        include(FetchContent)
        FetchContent_Declare(
          googletest
          URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
          DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        # For Windows: Prevent overriding the parent project's compiler/linker settings
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    enable_testing()
    add_subdirectory(tests)
endif()

if (YUME_BUILD_SELFTESTS)
    add_executable(ImeCoreSelfTests
        "tools/selftest/main.cpp"
        "src/platform/tsf/candidate_ui_element.cpp"
        "src/platform/tsf/idle_service.cpp"
        "src/platform/tsf/module_state.cpp"
        "src/platform/tsf/registration_helpers.cpp"
        "src/platform/tsf/text_service.cpp"
        "src/platform/tsf/text_service_activation.cpp"
        "src/platform/tsf/text_service_candidate_ui.cpp"
        "src/platform/tsf/text_service_candidate_window.cpp"
        "src/platform/tsf/text_service_commit.cpp"
        "src/platform/tsf/text_service_composition.cpp"
        "src/platform/tsf/text_service_config.cpp"
        "src/platform/tsf/text_service_dispatch.cpp"
        "src/platform/tsf/text_service_edit_session.cpp"
        "src/platform/tsf/text_service_focus.cpp"
        "src/platform/tsf/text_service_input.cpp"
        "src/platform/tsf/text_service_keymap.cpp"
        "src/platform/tsf/text_service_keymap_internal.cpp"
        "src/platform/tsf/text_service_output.cpp"
        "src/platform/tsf/text_service_sinks.cpp"
        "src/platform/tsf/text_service_state.cpp"
        "src/platform/tsf/text_service_ui_element.cpp"
        "src/platform/tsf/tsf_helpers.cpp"
        "src/ui/candidate_window/candidate_window.cpp"
        "src/ui/candidate_window/candidate_window_theme.cpp"
    )
    set_target_properties(ImeCoreSelfTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/selftest/$<CONFIG>")
    target_link_libraries(ImeCoreSelfTests PRIVATE ImeCore user32 gdi32 ole32 oleaut32 uuid)
    yume_copy_runtime_data_from(
        ImeCoreSelfTests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/dictionary/dictionary"
        "${CMAKE_CURRENT_SOURCE_DIR}/data/dictionary/db")
endif()

if (YUME_BUILD_BENCHMARKS)
    add_executable(ImeCoreBench
        "benchmarks/ime_core_bench.cpp"
    )
    set_target_properties(ImeCoreBench PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bench/$<CONFIG>")
    target_link_libraries(ImeCoreBench PRIVATE ImeCore)
    yume_copy_runtime_data(ImeCoreBench)
endif()
