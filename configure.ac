AC_PREREQ(2.59)

AC_INIT([dlfcn-win32], [1.0.0.99], [timothygu99@gmail.com])

AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_BUILD

AM_INIT_AUTOMAKE([foreign])
LT_INIT

# Set the flags here to make libtool catch the flags
CFLAGS='-Wall -O3 -fomit-frame-pointer'
AC_PROG_CC

AC_MSG_CHECKING([windows.h and win32 API])
AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
        #include <windows.h>
    ]], [[
        LoadLibrary(NULL);
    ]])
], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([C compiler is not able to link standard win32 program.])
])

MINGW_AC_MSVC_IMPORT_LIBS([LIBDL_MSVC_LIB], [libdl.lib])
AC_SUBST([LIBDL_MSVC_LIB])

test -n "${LIBDL_MSVC_LIB}" && \
    GEN_DEF="-Wl,--output-def,libdl.def" && \
    EXTRA_INSTALL="install-msvc-lib"
AC_SUBST([GEN_DEF])
AC_SUBST([EXTRA_INSTALL])

test -n 

AC_CONFIG_FILES(Makefile)
AC_OUTPUT