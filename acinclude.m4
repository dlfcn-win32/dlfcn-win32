# MSVC-compatibe import lib generation recipe
#
# Copyright (C) 2007, MinGW Project
# Written by Keith Marshall <keithmarshall@users.sourceforge.net>
#
#
# This is free software.  It is provided AS IS, in the hope that it may
# be useful, but WITHOUT WARRANTY OF ANY KIND, not even an IMPLIED WARRANTY
# of MERCHANTABILITY, nor of FITNESS FOR ANY PARTICULAR PURPOSE.
#
# Permission is granted to redistribute this software, either "as is" or
# in modified form, under the terms of the GNU Lesser General Public License,
# as published by the Free Software Foundation; either version 2.1, or (at
# your option) any later version.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this software; see the file COPYING.  If not, write to the
# Free Software Foundation, 51 Franklin St - Fifth Floor, Boston,
# MA 02110-1301, USA.

# Copied from http://sourceforge.net/p/mingw/regex/ci/e7b1d8be1cd5dcf0c654228c2af30fd769cac61d/tree/aclocal.m4
#
# Modifications:
# - License header:
#   - Short description of file added
#   - Changed 'see the file COPYING.lib' to 'see the file COPYING'
# - Removed all macros besides MINGW_AC_MSVC_IMPORT_LIBS
# - MINGW_AC_MSVC_IMPORT_LIBS:
#   - Renamed the option to --enable-msvc for compatibility with the old
#     configure script.
# - Cosmetics:
#   - Use '' instead of `'

# MINGW_AC_MSVC_IMPORT_LIBS( VARNAME, TARGET )
# --------------------------------------------
# Check for the availability of the MSVC 'lib' program.
# If it is found in $PATH, and the user has requested '--enable-msvc',
# then set the AC_SUBST variable VARNAME to TARGET, otherwise set VARNAME to
# the null string.
#
# If the user has requested '--enable-msvc', but MSVC 'lib' cannot be
# found, then 'configure' will print a warning; this will be suppressed, if
# '--enable-msvc' has not been requested.
#
AC_DEFUN([MINGW_AC_MSVC_IMPORT_LIBS],
[AC_ARG_ENABLE([msvc],
  AS_HELP_STRING([--enable-msvc],
  [enable building of MSVC compatible import libraries]),[],
  [enable_msvc=no])
 AC_CHECK_TOOL([MSVCLIB], [lib])
 if test "x$enable_msvc" = xyes && test -n "$MSVCLIB"
 then $1="$2"
 elif test "x$enable_msvc" = xyes
 then AC_MSG_WARN([no MSVC compatible 'lib' program found in \$PATH

  The MSVC 'lib' program is required to build MSVC compatible import libs.
  Since this program does not appear to be installed on this computer, MSVC
  compatible import libs will not be built; configuration will continue, but
  only MinGW format import libs will be included in the resultant build.

  If you wish to build a development kit which does include import libs for
  MSVC, in addition to those for MinGW, you should ensure that MSVC has been
  installed on this computer, and that \$PATH includes the directory in which
  its 'lib' program is located, then run 'configure' again.
  ])
 fi
 AC_SUBST([$1])dnl
])# MINGW_AC_MSVC_IMPORT_LIBS
