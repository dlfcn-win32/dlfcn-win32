AC_PREREQ(2.59)

AC_INIT([dlfcn-win32], [1.0.0.99], [timothygu99@gmail.com])

AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_BUILD

AM_INIT_AUTOMAKE([foreign])
LT_INIT

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

AC_CONFIG_FILES(Makefile)
AC_OUTPUT