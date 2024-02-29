#ifndef RAZORBACK_BOBINS_H
#define RAZORBACK_BOBINS_H
#include "safewindows.h"
#include <varargs.h>
#include <time.h>
#define CLOCK_REALTIME 0
struct timespec {
    time_t      tv_sec;     /* seconds */
	long tv_nsec;    /* microseconds */
};

extern int vasprintf (char **result, const char *format, va_list args);
extern int asprintf (char **buf, const char *fmt, ...);
extern int clock_gettime(int X, struct timespec *tv);
extern char * ctime_r (const time_t * tim_p, char * result);
#endif // RAZORBACK_BOBINS_H

