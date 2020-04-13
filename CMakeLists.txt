cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static linking of GLIBC + magic to make std::thread work.
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -static")

project(ev3dev-lang-cpp)
include(cmake/StandardProjectSettings.cmake)

add_library(project_options INTERFACE)

include(cmake/Sanitizers.cmake)
enable_sanitizers(project_options)

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
add_subdirectory(third_party/catch)
add_subdirectory(third_party/named_type)
add_subdirectory(third_party/fmt)
add_subdirectory(third_party/debug_assert)
add_subdirectory(third_party/type_safe)
add_subdirectory(third_party/gsl)
add_subdirectory(tests)
add_subdirectory(plotter_client)
add_subdirectory(plotter)

add_subdirectory(mqueue)
add_subdirectory(plotter_tests)
add_subdirectory(metaprogram)