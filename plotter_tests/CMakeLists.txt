# Test itself
add_executable(plotter_tests scheduler_test.cpp display_test.cpp)
target_link_libraries(plotter_tests PRIVATE project_warnings project_options catch_main plotter_lib)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Catch.cmake)

catch_discover_tests(
    plotter_tests
    TEST_PREFIX
    "pltter_test."
    EXTRA_ARGS
    -s
    --reporter=xml
    --out=plotter_tests.xml)