cmake_minimum_required(VERSION 2.8.11)

if (NOT DEFINED CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif ()

project (dlfcn-win32 C)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
include(Macros)

option(BUILD_SHARED_LIBS "shared/static libs" ON) 
option(BUILD_TESTS "tests?" OFF)

if(WIN32 AND NOT CMAKE_HOST_WIN32 AND CMAKE_CROSSCOMPILING AND BUILD_TESTS)
    add_auto_option(ENABLE_WINE "Enable running tests with wine" AUTO)
    find_program(WINE_EXECUTABLE wine)
    check_auto_option(ENABLE_WINE "wine support" WINE_EXECUTABLE "wine executable")
    if(ENABLE_WINE AND WINE_EXECUTABLE)
        set(WRAPPER ${WINE_EXECUTABLE})
        set(RUN_TESTS 1)
        message(STATUS "Support to run cross compiled tests - enabled")
    endif()
elseif(BUILD_TESTS)
    set(RUN_TESTS 1)
endif()

if(RUN_TESTS)
    enable_testing()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall)
endif()

add_subdirectory(src)

if (BUILD_TESTS)
    add_subdirectory(tests)
endif()
