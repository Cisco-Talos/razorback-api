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
#include <razorback/queue.h>
#include <razorback/thread.h>
#include <razorback/messages.h>
#include "runtime_config.h"


#include "init.h"

#define LOG_CONF_FILE "logging.conf"

static struct Queue *sg_logQueue = NULL;
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

    if (( sg_logQueue = Queue_Create(LOG_QUEUE, false, QUEUE_FLAG_SEND)) == NULL)
    {
        return false;
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
    
    if (log_dest != RZB_LOG_DEST_SYSLOG)
    {
        if (vasprintf (&msg, fmt, argp) == -1)
            return;
    }

    switch (log_dest)
    {
    case RZB_LOG_DEST_SYSLOG:
#ifdef _MSC_VER
		UNIMPLEMENTED();
#else
        vsyslog (level, fmt, argp);
#endif
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

SO_PUBLIC void
rzb_log_remote (uint8_t level, struct EventId *eventId, const char *fmt, ...)
{
    struct Message *message;
    char *msg = NULL;
    struct RazorbackContext *l_pContext;
	va_list argp;

    if (level > (unsigned) Config_getLogLevel())
    {
        return;
    }
    
    va_start (argp, fmt);

    if (vasprintf (&msg, fmt, argp) == -1)
        return;
    va_end (argp);
    
    l_pContext = Thread_GetCurrentContext ();

    if ((message = MessageLog_Initialize(l_pContext->uuidNuggetId,
            level, msg, eventId)) == NULL)
    {
        free (msg);
        return;
    }

    Queue_Put(sg_logQueue, message);
    message->destroy(message);
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


