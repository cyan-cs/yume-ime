# Core Engine Unit Tests target
add_executable(ImeCoreTests
    "../src/platform/tsf/text_service_config.cpp"
    "ime/engine_test.cpp"
    "ime/kana_transform_test.cpp"
    "platform/app_paths_test.cpp"
    "../src/platform/tsf/candidate_ui_element.cpp"
    "platform/candidate_ui_element_test.cpp"
    "platform/candidate_ui_model_test.cpp"
    "platform/logger_test.cpp"
    "platform/tsf_policy_test.cpp"
    "platform/text_service_config_test.cpp"
    "platform/windows_theme_test.cpp"
)

# Link against the GoogleTest framework and the core engine static library
target_link_libraries(ImeCoreTests
    PRIVATE
    gtest_main
    ImeCore
)

add_custom_command(TARGET ImeCoreTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ImeCoreTests>/data/dictionary/db"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:ImeCoreTests>/data/dictionary/dictionary"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ImeCoreTests>/data/dictionary/dictionary"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ImeCoreTests>/data/settings"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data/dictionary/dictionary"
        "$<TARGET_FILE_DIR:ImeCoreTests>/data/dictionary/dictionary"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../data/dictionary/db/user_db.bin"
        "$<TARGET_FILE_DIR:ImeCoreTests>/data/dictionary/db/user_db.bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../data/dictionary/db/black_db.bin"
        "$<TARGET_FILE_DIR:ImeCoreTests>/data/dictionary/db/black_db.bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../data/settings/text_service_config.json"
        "$<TARGET_FILE_DIR:ImeCoreTests>/data/settings/text_service_config.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../data/settings/logging_config.json"
        "$<TARGET_FILE_DIR:ImeCoreTests>/data/settings/logging_config.json")

add_test(NAME ImeCoreTests COMMAND ImeCoreTests)
