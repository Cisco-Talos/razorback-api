#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
m4_include([common_m4/versions.m4])
AC_INIT([razorback_api], API_VERSION_NUMBER, [https://sourceforge.net/apps/trac/razorbacktm/])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/rzb_api.c])
AC_CONFIG_HEADERS([config.h])
AH_BOTTOM([#include "rzb_visibility.h"])
AM_INIT_AUTOMAKE([foreign no-define tar-ustar])

APIVERSION=API_LIB_VERSION_NUMBER

# Pull in the checks for the build tools.
m4_include([common_m4/build_tools.m4])

# Pull in the platform specific build/configure environment options.
m4_include([common_m4/platform_flags.m4])

# Pull in the dependancies on on the API (use the nugget way).
m4_include([common_m4/api_deps.m4])

# Pull in the standard header and function checks.
m4_include([common_m4/standard_headers_functions.m4])

# Pull in the standard options for all components
m4_include([common_m4/standard_options.m4])

# Pull in the compiler customisation options for different compilers.
m4_include([common_m4/compiler_options.m4])

# Pull in the standard compiler and linker flags.
m4_include([common_m4/cflags.m4])

AC_SUBST(XCCFLAGS)
AC_SUBST(APIVERSION)
AC_SUBST(LIBS)
AC_SUBST(extra_incl)

API_INCLUDES='-I$(top_srcdir)/src -I$(top_srcdir)/utils'
UTILS_INCLUDES='-I$(top_srcdir)/utils'
UTILS_LIBRARIES='$(top_builddir)/utils/librazorback_utils.la'

AC_SUBST(API_INCLUDES)
AC_SUBST(UTILS_INCLUDES)
AC_SUBST(UTILS_LIBRARIES)

AC_CONFIG_FILES([ \
razorback.pc \
Makefile \
m4/Makefile \
src/Makefile \
utils/Makefile \
tests/Makefile \
])
AC_OUTPUT
