# Catch - build as static library
add_library(catch_main STATIC catch_main.cpp catch2.hpp)
target_link_libraries(catch_main PRIVATE project_warnings project_options)
target_include_directories(catch_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Test itself
add_executable(plotter_tests menu_test.cpp display_test.cpp)
target_link_libraries(plotter_tests PRIVATE project_warnings project_options catch_main plotter_lib)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Catch.cmake)

# catch_discover_tests(
#     plotter_tests
#     TEST_PREFIX
#     "unittests."
#     EXTRA_ARGS
#     -s
#     --reporter=xml
#     --out=plotter_tests.xml)

add_test(plotter_tests plotter_tests)

# add_executable(plotter_tests
#     menu_test.cpp
#     ${CMAKE_CURRENT_SOURCE_DIR}/../plotter/menu.cpp
#     )

# target_include_directories(plotter_tests PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/../plotter
#     ${CMAKE_CURRENT_SOURCE_DIR}/..
# )

# add_test(plotter_tests plotter_tests)
