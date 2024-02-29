#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <razorback/config_file.h>
#include <razorback/log.h>

#define LOG_CONF_FILE "rzb_logging.conf"

static RZB_LOG_DEST_t log_dest = RZB_LOG_DEST_ERR;
static int log_level = LOG_DEBUG;
static int log_facility;
static char *log_file;


static char level_strings[][9] = {
    "Debug",
    "Alert",
    "Critical",
    "Error",
    "Warning",
    "Notice",
    "Info",
    "Debug"
};

static bool parseLogDest (const char *, conf_int_t *);
static bool parseLogLevel (const char *, conf_int_t *);
static bool parseLogFacility (const char *, conf_int_t *);

static RZBConfCallBack destCallBack = {
    &parseLogDest
};

static RZBConfCallBack levelCallBack = {
    &parseLogLevel
};

static RZBConfCallBack facilityCallBack = {
    &parseLogFacility
};

static RZBConfKey_t logging_config[] = {
    {"LOG_DEST", RZB_CONF_KEY_TYPE_PARSED_STRING, &log_dest, &destCallBack},
    {"LOG_LEVEL", RZB_CONF_KEY_TYPE_PARSED_STRING, &log_level,
     &levelCallBack},
    {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
};

static RZBConfKey_t syslog_config[] = {
    {"SYSLOG_FACILITY", RZB_CONF_KEY_TYPE_PARSED_STRING, &log_facility,
     &facilityCallBack},
    {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
};

static RZBConfKey_t logfile_config[] = {
    {"LOG_FILE", RZB_CONF_KEY_TYPE_STRING, &log_file, NULL},
    {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
};

SO_PUBLIC bool __attribute__ ((constructor)) configureLogging ()
{
    if (!readMyConfig (NULL, LOG_CONF_FILE, logging_config))
        return false;

    switch (log_dest)
    {
    case RZB_LOG_DEST_SYSLOG:
        if (!readMyConfig (NULL, LOG_CONF_FILE, syslog_config))
            return false;
        openlog (NULL, LOG_PID, log_facility);
        break;
    case RZB_LOG_DEST_FILE:
        if (!readMyConfig (NULL, LOG_CONF_FILE, logfile_config))
            return false;

        break;
    case RZB_LOG_DEST_ERR:
        break;
    }
    return true;
}

SO_PUBLIC void
rzb_perror (const char *fmt)
{
    rzb_log (LOG_ERR, fmt, strerror (errno));
}

SO_PUBLIC void
rzb_log (unsigned level, const char *fmt, ...)
{
    char *msg = NULL;
    if (level > (unsigned) log_level)
    {
        return;
    }
    va_list argp;
    va_start (argp, fmt);

    if (log_dest != RZB_LOG_DEST_SYSLOG)
    {
        if (vasprintf (&msg, fmt, argp) == -1)
            return;
    }

    switch (log_dest)
    {
    case RZB_LOG_DEST_SYSLOG:
        vsyslog (level, fmt, argp);
        break;
    case RZB_LOG_DEST_FILE:
        break;
    case RZB_LOG_DEST_ERR:
    default:
        fprintf (stderr, "%s: %s\n", level_strings[level], msg);
        break;
    }
    va_end (argp);
    if (msg != NULL)
        free (msg);
}

SO_PUBLIC int
rzb_get_log_level ()
{
    return log_level;
}

SO_PUBLIC RZB_LOG_DEST_t
rzb_get_log_dest ()
{
    return log_dest;
}

SO_PUBLIC void
rzb_debug_logging ()
{
    log_level = LOG_DEBUG;
    log_dest = RZB_LOG_DEST_ERR;
}

static bool
parseLogDest (const char *string, conf_int_t * val)
{
    if (!strncasecmp (string, "syslog", 6))
    {
        *val = RZB_LOG_DEST_SYSLOG;
        return true;
    }
    else if (!strncasecmp (string, "stderr", 6))
    {
        *val = RZB_LOG_DEST_ERR;
        return true;
    }
    else if (!strncasecmp (string, "file", 4))
    {
        *val = RZB_LOG_DEST_FILE;
        return true;
    }
    return false;
}

static bool
parseLogLevel (const char *string, conf_int_t * val)
{
    if (!strncasecmp (string, "emergency", 9))
    {
        *val = LOG_EMERG;
        return true;
    }
    else if (!strncasecmp (string, "alert", 5))
    {
        *val = LOG_ALERT;
        return true;
    }
    else if (!strncasecmp (string, "critical", 8))
    {
        *val = LOG_CRIT;
        return true;
    }
    else if (!strncasecmp (string, "error", 5))
    {
        *val = LOG_ERR;
        return true;
    }
    else if (!strncasecmp (string, "warning", 7))
    {
        *val = LOG_WARNING;
        return true;
    }
    else if (!strncasecmp (string, "notice", 6))
    {
        *val = LOG_NOTICE;
        return true;
    }
    else if (!strncasecmp (string, "info", 4))
    {
        *val = LOG_INFO;
        return true;
    }
    else if (!strncasecmp (string, "debug", 5))
    {
        *val = LOG_DEBUG;
        return true;
    }
    return false;
}

static bool
parseLogFacility (const char *string, conf_int_t * val)
{
    if (!strncasecmp (string, "daemon", 6))
    {
        *val = LOG_DAEMON;
        return true;
    }
    else if (!strncasecmp (string, "user", 4))
    {
        *val = LOG_USER;
        return true;
    }
    else if (!strncasecmp (string, "local0", 6))
    {
        *val = LOG_LOCAL0;
        return true;
    }
    else if (!strncasecmp (string, "local1", 6))
    {
        *val = LOG_LOCAL1;
        return true;
    }
    else if (!strncasecmp (string, "local2", 6))
    {
        *val = LOG_LOCAL2;
        return true;
    }
    else if (!strncasecmp (string, "local3", 6))
    {
        *val = LOG_LOCAL3;
        return true;
    }
    else if (!strncasecmp (string, "local4", 6))
    {
        *val = LOG_LOCAL4;
        return true;
    }
    else if (!strncasecmp (string, "local5", 6))
    {
        *val = LOG_LOCAL5;
        return true;
    }
    else if (!strncasecmp (string, "local6", 6))
    {
        *val = LOG_LOCAL6;
        return true;
    }
    else if (!strncasecmp (string, "local7", 6))
    {
        *val = LOG_LOCAL7;
        return true;
    }
    return false;
}
