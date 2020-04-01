# NamedType lib
add_library(named_type_lib STATIC named_type/named_type.hpp named_type/named_type_impl.hpp named_type/underlying_functionalities.hpp)
target_include_directories(named_type_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(named_type_lib PROPERTIES LINKER_LANGUAGE CXX)