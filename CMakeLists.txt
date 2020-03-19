cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXE_LINKER_FLAGS "-static")

project(ev3dev-lang-cpp)

add_library(project_options INTERFACE)
include(cmake/StandardProjectSettings.cmake)

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)

# standard compiler warnings
include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)

set(EV3DEV_PLATFORM "EV3" CACHE STRING "Target ev3dev platform (EV3/BRICKPI/BRICKPI3/PISTORMS)")
set_property(CACHE EV3DEV_PLATFORM PROPERTY STRINGS "EV3" "BRICKPI" "BRICKPI3" "PISTORMS")

add_library(ev3dev STATIC ev3dev.cpp)
add_library(ev3dev::ev3dev ALIAS ev3dev) # to match exported target

target_compile_definitions(ev3dev PUBLIC _GLIBCXX_USE_NANOSLEEP EV3DEV_PLATFORM_${EV3DEV_PLATFORM})
target_link_libraries(ev3dev PUBLIC pthread)
target_include_directories(ev3dev PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

function(add_ev3_executable target sources)
    add_executable(${target} ${sources})
    target_link_libraries(${target} ev3dev project_warnings project_options)
endfunction()

enable_testing()
add_subdirectory(tests)
add_subdirectory(plotter)
add_subdirectory(plotter_tests)
add_subdirectory(metaprogram)