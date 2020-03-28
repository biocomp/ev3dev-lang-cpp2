# Catch - build as static library
add_library(catch_main STATIC catch_main.cpp catch2.hpp)
target_link_libraries(catch_main PRIVATE project_warnings project_options)
target_include_directories(catch_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(catch_main PUBLIC CATCH_CONFIG_CONSOLE_WIDTH=300)