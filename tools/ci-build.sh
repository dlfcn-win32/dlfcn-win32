#!/bin/bash

set -euo pipefail
set -x

# ci_buildsys:
# Build system under test: Makefile or cmake
: "${ci_buildsys:=cmake}"

# ci_target:
# target to build for
: "${ci_target:=x86_64-linux-gnu-gcc}"

# ci_variant:
# variant to build e.g shared, static
: "${ci_variant:=shared}"

if test -v CROSS_COMPILE; then
    ci_target=${CROSS_COMPILE%-}
fi

case "$ci_buildsys" in
    (Makefile)
        case "$ci_target" in
            (*mingw32*)
                ./configure --enable-shared --enable-static --enable-wine --cross-prefix=${ci_target}-
                make
                make test
                ;;
        esac
        ;;

    (cmake)
        cmake_options="
            --no-warn-unused-cli
            -DBUILD_TESTS=1
            -DCMAKE_BUILD_TYPE=RelWithDebInfo
        "
        case "$ci_variant" in
            (shared)
                cmake_options+=" -DBUILD_SHARED_LIBS=ON"
                ;;
            (static)
                cmake_options+=" -DBUILD_SHARED_LIBS=OFF"
                ;;
        esac

        cmake --version

        # create build dir
        rm -rf ci-build-${ci_variant}-${ci_target}
        mkdir -p ci-build-${ci_variant}-${ci_target}
        cd ci-build-${ci_variant}-${ci_target}

        case "$ci_target" in
            (*mingw32*)
                cmake \
                    -DCMAKE_FIND_ROOT_PATH=$(${ci_target}-gcc --print-sysroot)/${ci_target}       \
                    -DCMAKE_C_COMPILER=$(which ${ci_target}-gcc) \
                    -DCMAKE_SYSTEM_PROCESSOR=${ci_target%-*-*}   \
                    -DCMAKE_CROSSCOMPILING=TRUE                  \
                    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER    \
                    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY     \
                    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY     \
                    -DCMAKE_SYSTEM_NAME=Windows                  \
                    -DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/wine              \
                    $cmake_options \
                    ..
                ;;
            (*linux*)
                cmake \
                    $cmake_options \
                    ..
                ;;
        esac

        make
        ctest --output-on-failure
        make install DESTDIR=$(pwd)/DESTDIR
        ;;
esac

# vim:set sw=4 sts=4 et:
