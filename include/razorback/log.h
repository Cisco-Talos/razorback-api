/** @file log.h
 * The Razorback Logging API.
 * 
 * Log levels avaliable are the same as the standard syslog log levels and
 * definitions are imported from syslog.
 *
 * Levels are:
 *      LOG_EMERGE
 *      LOG_ALERT
 *      LOG_CRIT
 *      LOG_ERR
 *      LOG_WARNING
 *      LOG_NOTICE
 *      LOG_INFO
 *      LOG_DEBUG
 *
 */
#ifndef RAZORBACK_LOG_H
#define RAZORBACK_LOG_H
#include <syslog.h>

#include <razorback/types.h>


/** Log Destinations
 */
typedef enum
{
    RZB_LOG_DEST_FILE,          ///< Write to a file
    RZB_LOG_DEST_SYSLOG,        ///< Write to syslog
    RZB_LOG_DEST_ERR,           ///< Write to stderr
} RZB_LOG_DEST_t;

/** Log a message to the system message log
 * @param level The message level as defined in syslog.h
 * @param fmt The format string for the message
 * @param ... The data to be formatted.
 */
void rzb_log (unsigned level, const char *fmt, ...);
extern void
rzb_log_remote (uint8_t level, struct EventId *eventId, const char *fmt, ...);

/** Log a standard error.
 * @param message the log message associated with the error.
 */
void rzb_perror (const char *message);

/** Get the currently configured log level
 * @return One of the log levels defined in syslog.h
 */
int rzb_get_log_level ();

/** Get the currently configured log destination
 * @return The current log distination.
 */
RZB_LOG_DEST_t rzb_get_log_dest ();

/** Set logging to debug mode.
 */
void rzb_debug_logging ();

#endif // RAZORBACK_LOG_H
