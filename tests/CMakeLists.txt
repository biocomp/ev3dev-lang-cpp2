add_executable(api_tests api_tests.cpp)

target_link_libraries(api_tests PRIVATE ev3dev project_options project_warnings)

target_compile_definitions(api_tests PRIVATE
    SYS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/fake-sys/arena"
    FAKE_SYS="${CMAKE_CURRENT_SOURCE_DIR}/fake-sys")

add_test(api_tests api_tests)
