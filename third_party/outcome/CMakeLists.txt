# Outcome lib
add_library(outcome_lib STATIC outcome/outcome.hpp)
target_include_directories(outcome_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(outcome_lib PROPERTIES LINKER_LANGUAGE CXX)