
function(enable_sanitizers project_name)

    message ("## 1 ENABLE_SANITIZER_ADDRESS = ${ENABLE_SANITIZER_ADDRESS}")

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL
                                             "Clang")
    option(ENABLE_COVERAGE "Enable coverage reporting for gcc/clang" FALSE)

    if(ENABLE_COVERAGE)
      target_compile_options(project_options INTERFACE --coverage -O0 -g)
      target_link_libraries(project_options INTERFACE --coverage)
    endif()

    set(SANITIZERS "")

    message ("## 2 ENABLE_SANITIZER_ADDRESS = ${ENABLE_SANITIZER_ADDRESS}")

    option(ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" FALSE)
    if(ENABLE_SANITIZER_ADDRESS)
      list(APPEND SANITIZERS "address")
    endif()

    option(ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" FALSE)
    if(ENABLE_SANITIZER_MEMORY)
      list(APPEND SANITIZERS "memory")
    endif()

    option(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR
           "Enable undefined behavior sanitizer" FALSE)
    if(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR)
      list(APPEND SANITIZERS "undefined")
    endif()

    option(ENABLE_SANITIZER_THREAD "Enable thread sanitizer" FALSE)
    if(ENABLE_SANITIZER_THREAD)
      list(APPEND SANITIZERS "thread")
    endif()

    message ("## 1 SANITIZERS = ${SANITIZERS}, LIST_OF_SANITIZERS = ${LIST_OF_SANITIZERS}")
    string(REPLACE ";" "," LIST_OF_SANITIZERS "${SANITIZERS}")
    #JOINN(${SANITIZERS} ";" LIST_OF_SANITIZERS)
    message ("## 2 SANITIZERS = ${SANITIZERS}, LIST_OF_SANITIZERS = ${LIST_OF_SANITIZERS}")

  endif()

  message("Checking sanitizers: ${LIST_OF_SANITIZERS}")
  if(LIST_OF_SANITIZERS)
    message("Gonna use them sanitizers1: ${LIST_OF_SANITIZERS}")
    if(NOT "${LIST_OF_SANITIZERS}" STREQUAL "")
      message("Gonna use them sanitizers2: ${LIST_OF_SANITIZERS}")
      target_compile_options(${project_name}
                             INTERFACE -fsanitize=${LIST_OF_SANITIZERS})
      target_link_libraries(${project_name}
                            INTERFACE -fsanitize=${LIST_OF_SANITIZERS})
    endif()
  endif()

endfunction()
