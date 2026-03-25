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

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <varargs.h>
#include "bobins.h"
#endif

#include <razorback/debug.h>
#include <razorback/log.h>
#include "runtime_config.h"


#include "init.h"
#include "telemetry.h"

#define LOG_CONF_FILE "logging.conf"

static char level_strings[][9] = {
    "Emerg",
    "Alert",
    "Critical",
    "Error",
    "Warning",
    "Notice",
    "Info",
    "Debug"
};
static uint64_t sg_logMask = LOG_C_ALL;


bool
configureLogging (void)
{
    char *logMask = NULL;
    char *parseEnd = NULL;
#ifdef _MSC_VER
#else //_MSC_VER
    if (Config_getLogDest() == RZB_LOG_DEST_SYSLOG)
        openlog (NULL, LOG_PID, Config_getLogFacility());
#endif //_MSC_VER

    logMask = getenv("RZB_LOG_MASK");
    if (logMask != NULL) {
        sg_logMask = strtoull(logMask, &parseEnd, 16);
        if (*parseEnd != '\0') {
            rzb_log(LOG_ERR, LOG_C_CONFIG,
                "Invalid RZB_LOG_MASK value '%s', using default", logMask);
            sg_logMask = LOG_C_ALL;
        }
    }

    return true;
}

SO_PUBLIC void
rzb_perror (uint64_t component, const char *fmt)
{
    rzb_log (LOG_ERR, component, fmt, strerror (errno));
}

SO_PUBLIC void
rzb_log (unsigned level, uint64_t compontent, const char *fmt, ...)
{
    char *msg = NULL;
    bool emitTelemetry = false;
    bool formatted = false;

    if ((level == LOG_DEBUG) && (sg_logMask & compontent) == 0) {
        return;
    }

    va_list argp;
    RZB_LOG_DEST_t log_dest = Config_getLogDest();
    if (level > (unsigned) Config_getLogLevel())
    {
        return;
    }

    va_start (argp, fmt);
    emitTelemetry = Telemetry_IsLogEnabled();

    if (log_dest != RZB_LOG_DEST_SYSLOG || emitTelemetry)
    {
        va_list formatArgs;

        va_copy(formatArgs, argp);
        if (vasprintf (&msg, fmt, formatArgs) != -1)
            formatted = true;
        va_end(formatArgs);
    }

    switch (log_dest)
    {
    case RZB_LOG_DEST_SYSLOG:
#ifdef _MSC_VER
        UNIMPLEMENTED();
#else
        if (formatted)
            syslog (level, "%s", msg);
        else
            vsyslog (level, fmt, argp);
#endif
        break;
    case RZB_LOG_DEST_FILE:
        break;
    case RZB_LOG_DEST_ERR:
    default:
        if (formatted)
            fprintf (stderr, "%s: %s\n", level_strings[level], msg);
        break;
    }
    if (emitTelemetry && formatted)
        Telemetry_LogMessage(level, compontent, msg);
    va_end (argp);
    if (msg != NULL)
        free (msg);
}

SO_PUBLIC int
rzb_get_log_level ()
{
    return Config_getLogLevel();
}

SO_PUBLIC RZB_LOG_DEST_t
rzb_get_log_dest ()
{
    return Config_getLogDest();
}

SO_PUBLIC void
rzb_debug_logging ()
{
    Config_setLogLevel(LOG_DEBUG);
    Config_setLogDest(RZB_LOG_DEST_ERR);
}
