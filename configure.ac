# Copyright (c) 2011-2026 Cisco Systems, Inc.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.

AC_PREREQ([2.61])
m4_include([common_m4/versions.m4])
AC_INIT([api], API_VERSION_NUMBER, [https://sourceforge.net/apps/trac/razorbacktm/])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/thread.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign no-define tar-ustar subdir-objects])

APIVERSION=API_LIB_VERSION_NUMBER

# Pull in the checks for the build tools.
m4_include([common_m4/build_tools.m4])
AC_PROG_CXX
LT_LANG([C++])

# Pull in the platform specific build/configure environment options.
m4_include([common_m4/platform_flags.m4])

# Pull in the dependancies on on the API (use the nugget way).
m4_include([common_m4/api_deps.m4])

# Pull in the standard header and function checks.
m4_include([common_m4/standard_headers_functions.m4])

# Pull in the standard options for all components
m4_include([common_m4/standard_options.m4])

AC_ARG_ENABLE([cnc_debug], [AS_HELP_STRING([--enable-cnc-debug],[Enable command and control debug])], [enable_cnc_debug=$enableval], [enable_cnc_debug="no"])
if test "x$enable_cnc_debug" = "xyes"; then
    AC_DEFINE([CNC_DEBUG], 1, [Command and Control Debug Mode])
fi

AC_ARG_ENABLE([stomp_debug], [AS_HELP_STRING([--enable-stomp-debug],[Enable stomp debug])], [enable_stomp_debug=$enableval], [enable_stomp_debug="no"])
if test "x$enable_stomp_debug" = "xyes"; then
    AC_DEFINE([STOMP_DEBUG], 1, [Stomp Debug Mode])
fi

AC_ARG_ENABLE([magic_debug], [AS_HELP_STRING([--enable-magic-debug],[Enable magic debug])], [enable_magic_debug=$enableval], [enable_magic_debug="no"])
if test "x$enable_magic_debug" = "xyes"; then
    AC_DEFINE([MIME_MAGIC_DEBUG], 1, [Magic Debug Mode])
fi

AC_ARG_ENABLE([list_debug], [AS_HELP_STRING([--enable-list-debug],[Enable list debug])], [enable_list_debug=$enableval], [enable_list_debug="no"])
if test "x$enable_list_debug" = "xyes"; then
    AC_DEFINE([LIST_DEBUG], 1, [List Debug Mode])
fi

#m4_include([libssh/configure.m4])

AC_CHECK_HEADERS([sys/endian.h], [], [SYS_ENDIAN_HEADER="no"]) 
# Pull in the compiler customisation options for different compilers.
m4_include([common_m4/compiler_options.m4])

# Pull in the standard compiler and linker flags.
m4_include([common_m4/cflags.m4])

AC_DEFINE_UNQUOTED(NUGGET_SO_VERSION, "[NUGGET_VERSION_NUMBER]", ["So Version Of Nuggets"])
AS_AC_EXPAND(LIBDIR, "$libdir")
libdir=$LIBDIR

AC_DEFINE_UNQUOTED(LIB_DIR, "$LIBDIR", ["Lib dir"])

PKG_CHECK_MODULES([CHECK], [check >= 0.15.2], [have_check=yes], [have_check=no])
AM_CONDITIONAL([HAVE_CHECK], [test "x$have_check" = "xyes"])
AS_IF([test "x$have_check" != "xyes"], [
    AC_MSG_WARN([libcheck not found; unit tests will not be built])
])

if test "x$prefix" != "xNONE"; then
    PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$prefix/lib64/pkgconfig:$PKG_CONFIG_PATH"
    export PKG_CONFIG_PATH
fi

PKG_CHECK_MODULES([OPENTELEMETRY_CPP],
                  [opentelemetry_trace opentelemetry_exporter_otlp_http],
                  [have_opentelemetry_cpp=yes],
                  [have_opentelemetry_cpp=no])
AS_IF([test "x$have_opentelemetry_cpp" != "xyes"], [
    AC_MSG_ERROR([opentelemetry-cpp is required; install opentelemetry_trace and opentelemetry_exporter_otlp_http])
])

PKG_CHECK_MODULES([PROTOBUF], [protobuf], [], [
    AC_MSG_ERROR([protobuf is required to link opentelemetry-cpp OTLP exporters])
])
OPENTELEMETRY_CPP_LIBS="$OPENTELEMETRY_CPP_LIBS -lopentelemetry_proto -lopentelemetry_http_client_curl $PROTOBUF_LIBS"

AC_PATH_PROG([JV], [jv], [])
AM_CONDITIONAL([HAVE_JV], [test -n "$JV"])
AS_IF([test -z "$JV"], [
    AC_MSG_WARN([jv command not found in PATH])
])

AC_SUBST(XCCFLAGS)
AC_SUBST(APIVERSION)
AC_SUBST(JV)
AC_SUBST(LIBS)
AC_SUBST(extra_incl)
AC_SUBST(OPENTELEMETRY_CPP_CFLAGS)
AC_SUBST(OPENTELEMETRY_CPP_LIBS)

AC_CONFIG_FILES([ \
razorback.pc \
Makefile \
m4/Makefile \
etc/Makefile \
include/Makefile \
src/Makefile \
share/Makefile \
share/razorback/Makefile \
tests/Makefile \
])
AC_OUTPUT
