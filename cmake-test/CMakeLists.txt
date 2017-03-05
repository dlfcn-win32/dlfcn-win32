# Simple CMake project to test the use of dlfcn-win32
# imported target. The test compiled is the same compiled
# as part of the main dlfcn-win32 project 

cmake_minimum_required(VERSION 3.0)

find_package(dlfcn-win32 REQUIRED)

add_library(testdll SHARED ../testdll.c)
set_target_properties(testdll PROPERTIES PREFIX "")
add_executable(t_dlfcn ../test.c)
target_link_libraries(t_dlfcn dlfcn-win32::dl)
enable_testing()
add_test(NAME t_dlfcn COMMAND t_dlfcn)
