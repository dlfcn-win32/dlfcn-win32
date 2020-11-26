include_directories(../src)

add_library(testdll SHARED testdll.c)
set_target_properties(testdll PROPERTIES PREFIX "")

add_library(testdll2 SHARED testdll2.c)
set_target_properties(testdll2 PROPERTIES PREFIX "")
target_link_libraries(testdll2 dl)

add_library(testdll3 SHARED testdll3.c)
set_target_properties(testdll3 PROPERTIES PREFIX "")

add_executable(t_dlfcn test.c)
target_link_libraries(t_dlfcn dl)
if(RUN_TESTS)
    add_test(NAME t_dlfcn COMMAND ${WRAPPER} $<TARGET_FILE:t_dlfcn> WORKING_DIRECTORY $<TARGET_FILE_DIR:t_dlfcn>)
endif()
