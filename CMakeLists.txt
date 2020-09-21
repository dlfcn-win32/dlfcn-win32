cmake_minimum_required(VERSION 2.8.11)

if (NOT DEFINED CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif ()

project (dlfcn-win32 C)

option(BUILD_SHARED_LIBS "shared/static libs" ON) 
option(BUILD_TESTS "tests?" OFF)

add_subdirectory(src)

if (BUILD_TESTS)
  add_subdirectory(tests)
endif()
