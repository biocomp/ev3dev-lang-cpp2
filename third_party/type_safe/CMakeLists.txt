# TypeSafe lib
add_library(typesafe_lib STATIC)
target_include_directories(typesafe_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/type_safe/include)
set_target_properties(typesafe_lib PROPERTIES LINKER_LANGUAGE CXX)