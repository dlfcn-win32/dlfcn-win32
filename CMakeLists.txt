cmake_minimum_required(VERSION 2.8)

if (NOT DEFINED CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif ()

project (dlfcn-win32 C)

option(BUILD_SHARED_LIBS "shared/static libs" ON) 
option(BUILD_TESTS "tests?" OFF)

set(headers dlfcn.h)
set(sources dlfcn.c)

if (BUILD_SHARED_LIBS)
  add_definitions(-DSHARED)
endif (BUILD_SHARED_LIBS)

add_library(dl ${sources})
target_link_libraries(dl psapi)

install (TARGETS dl EXPORT dlfcn-win32-targets
                    RUNTIME DESTINATION bin
                    LIBRARY DESTINATION lib${LIB_SUFFIX}
                    ARCHIVE DESTINATION lib${LIB_SUFFIX})

install (FILES ${headers} DESTINATION include)

# If CMake version is greater than or equal to 2.8.11
# also install the cmake configuration files to simplify
# the use of dlfcn-win32 in CMake
if(NOT ${CMAKE_VERSION} VERSION_LESS "2.8.11")
    # Correctly export the location of installed includes in the target 
    target_include_directories(dl INTERFACE $<INSTALL_INTERFACE:include>)

    # Export the targets  (build tree)
    export(EXPORT dlfcn-win32-targets
           FILE "${CMAKE_CURRENT_BINARY_DIR}/dlfcn-win32-targets.cmake"
           NAMESPACE dlfcn-win32::
    )
    
    # Write the CMake config file
    set(CMAKE_CONF_INSTALL_DIR share/dlfcn-win32)
    set(INCLUDE_INSTALL_DIR include)
    include(CMakePackageConfigHelpers)
    configure_package_config_file(dlfcn-win32-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/dlfcn-win32-config.cmake
                                  INSTALL_DESTINATION ${CMAKE_CONF_INSTALL_DIR}
                                  PATH_VARS INCLUDE_INSTALL_DIR
                                  NO_CHECK_REQUIRED_COMPONENTS_MACRO)

    # Install the targets (install)
    install(EXPORT dlfcn-win32-targets
            FILE dlfcn-win32-targets.cmake
            NAMESPACE dlfcn-win32::
            DESTINATION ${CMAKE_CONF_INSTALL_DIR})

    # Install the CMake config file
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dlfcn-win32-config.cmake
            DESTINATION ${CMAKE_CONF_INSTALL_DIR})
endif()

if (BUILD_TESTS)
  enable_testing()
  add_library(testdll SHARED testdll.c)
  set_target_properties(testdll PROPERTIES PREFIX "")
  add_executable(t_dlfcn test.c)
  target_link_libraries(t_dlfcn dl)
  add_test (NAME t_dlfcn COMMAND t_dlfcn)
endif ()
