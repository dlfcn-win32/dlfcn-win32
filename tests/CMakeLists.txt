
if(WIN32)
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

    add_test(NAME t_dlfcn COMMAND t_dlfcn WORKING_DIRECTORY $<TARGET_FILE_DIR:t_dlfcn> )
endif()

add_executable(test-dladdr test-dladdr.c)
target_link_libraries(test-dladdr dl)
if(UNIX)
    set_target_properties(test-dladdr PROPERTIES COMPILE_FLAGS "-Wl,--export-dynamic -fpie")
endif()
if(WIN32 AND NOT BUILD_SHARED_LIBS)
    if(MSVC)
        set_property(TARGET test-dladdr APPEND_STRING PROPERTY LINK_FLAGS "/EXPORT:dlopen /EXPORT:dladdr")
    else()
        set_property(TARGET test-dladdr APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--export-all-symbols")
    endif()
endif()

install(TARGETS test-dladdr EXPORT dlfcn-win32-targets RUNTIME DESTINATION bin)

add_test(NAME test-dladdr COMMAND test-dladdr WORKING_DIRECTORY $<TARGET_FILE_DIR:test-dladdr>)
