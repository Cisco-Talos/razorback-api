/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

/** @file visibility.h
 * Library symbol visibility macro.
 */
#ifndef _RAZORBACK_VISIBILTY_H
#define _RAZORBACK_VISIBILTY_H

#if defined _WIN32 || defined __CYGWIN__
#  ifdef BUILDING_SO
#    ifdef __GNUC__
#      define SO_PUBLIC __attribute__((dllexport))
#    else
#      define SO_PUBLIC __declspec(dllexport)   // Note: actually gcc seems to also supports this syntax.
#    endif
#  else
#    ifdef __GNUC__
#      define SO_PUBLIC __attribute__((dllimport))
#    else
#      define SO_PUBLIC __declspec(dllimport)   // Note: actually gcc seems to also supports this syntax.
#    endif
#  endif
#  define DLL_LOCAL
#  define PRINTF_FUNC(x,y)
#else
#  if __GNUC__ >= 4
#    define SO_PUBLIC __attribute__ ((visibility("default")))
#    define SO_LOCAL  __attribute__ ((visibility("hidden")))
#  else
#    define SO_PUBLIC
#    define SO_LOCAL
#  endif
#  define PRINTF_FUNC(x,y) __attribute__((format (printf, x, y)))
#endif

#endif /* _RAZORBACK_VISIBILTY_H */
