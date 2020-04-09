add_library(mqueue_lib STATIC message_queue.cpp mqueue/message_queue.h)
target_include_directories(mqueue_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mqueue_lib project_warnings project_options)
