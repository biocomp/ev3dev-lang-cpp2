add_executable(api_tests api_tests.cpp)

target_link_libraries(api_tests PRIVATE ev3dev project_options project_warnings catch_main)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Catch.cmake)

catch_discover_tests(
    api_tests
    TEST_PREFIX
    "api_tests."
    EXTRA_ARGS
    -s
    --reporter=xml
    --out=plotter_tests.xml)