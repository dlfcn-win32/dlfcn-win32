#!/bin/bash

set -euo pipefail
set -x

# ci_buildsys:
# Build system under test: Makefile or cmake
: "${ci_buildsys:=cmake}"

# ci_target:
# target to build for
: "${ci_target:=${CROSS_COMPILE%-}}"

case "$ci_buildsys" in
    (Makefile)
        ./configure --enable-shared --enable-static --enable-wine --cross-prefix=${ci_target}-
        make
        make test
        ;;

    (cmake)
        cmake --version
        rm -rf build
        mkdir build
        cd build
        cmake \
            --no-warn-unused-cli                         \
            -DCMAKE_FIND_ROOT_PATH=$(${ci_target}-gcc --print-sysroot)/${ci_target}       \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo            \
            -DCMAKE_C_COMPILER=$(which ${ci_target}-gcc) \
            -DCMAKE_SYSTEM_PROCESSOR=${ci_target%-*-*}   \
            -DCMAKE_CROSSCOMPILING=TRUE                  \
            -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER    \
            -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY     \
            -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY     \
            -DCMAKE_SYSTEM_NAME=Windows                  \
            -DBUILD_TESTS=1                              \
            -DENABLE_WINE=ON                             \
            -DWINE_EXECUTABLE=/usr/bin/wine              \
            ..
        make
        ctest --output-on-failure
        make install DESTDIR=$(pwd)/DESTDIR
        ;;
esac

# vim:set sw=4 sts=4 et:
