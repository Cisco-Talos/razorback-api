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

#ifndef RAZORBACK_BOBINS_H
#define RAZORBACK_BOBINS_H
#include "safewindows.h"
#include <varargs.h>
#include <time.h>
#include <razorback/visibility.h>
#define CLOCK_REALTIME 0
struct timespec {
    time_t      tv_sec;     /* seconds */
	long tv_nsec;    /* microseconds */
};

SO_PUBLIC extern int vasprintf (char **result, const char *format, va_list args);
SO_PUBLIC extern int __cdecl asprintf (char **buf, const char *fmt, ...);
SO_PUBLIC extern int clock_gettime(int X, struct timespec *tv);
SO_PUBLIC extern char * ctime_r (const time_t * tim_p, char * result);
#endif // RAZORBACK_BOBINS_H

