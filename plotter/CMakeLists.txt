add_library(plotter_lib STATIC menu.cpp menu.h display.cpp display.h)
target_include_directories(plotter_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(plotter_lib PUBLIC ev3dev project_warnings project_options named_type_lib fmt::fmt)

add_executable(plotter main.cpp)
target_link_libraries(plotter PRIVATE plotter_lib ev3dev)
