add_executable(plotter_client main.cpp)
target_link_libraries(plotter_client PRIVATE project_warnings project_options fmt::fmt mqueue_lib)
