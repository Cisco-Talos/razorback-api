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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#define LOG_EMERG 0
#define LOG_EMERGE 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7

#else //_MSC_VER
#include <syslog.h>
#endif

#include <razorback/visibility.h>
#include <razorback/types.h>


/** Log Destinations
 */
typedef enum
{
    RZB_LOG_DEST_FILE,          ///< Write to a file
    RZB_LOG_DEST_SYSLOG,        ///< Write to syslog
    RZB_LOG_DEST_ERR,           ///< Write to stderr
} RZB_LOG_DEST_t;

#define LOG_C_ALL     0xFFFFFFFFFFFFFFFF
#define LOG_C_CORE        (1<<0)
#define LOG_C_NETWORK     (1<<1)
#define LOG_C_STOMP       (1<<2)
#define LOG_C_QUEUE       (1<<3)
#define LOG_C_TRANSFER    (1<<4)
#define LOG_C_CNC         (1<<5)
#define LOG_C_CONFIG      (1<<6)
#define LOG_C_MAGIC       (1<<7)
#define LOG_C_LIST        (1<<8)
#define LOG_C_JSON        (1<<9)
#define LOG_C_DISPATCHER  (1<<10)
#define LOG_C_NUGGET      (1<<11)

/**
 * Log a message to the system message log.
 * @param level The message level as defined in syslog.h.
 * @param component The component the message is associated with.
 * @param fmt The format string for the message.
 * @param ... The data to be formatted.
 * @return No return value.
 */
SO_PUBLIC extern void rzb_log(unsigned level, uint64_t component, const char *fmt, ...);

/**
 * Log a standard error.
 * @param component The component the error is associated with.
 * @param message the log message associated with the error.
 * @return No return value.
 */
SO_PUBLIC extern void rzb_perror(uint64_t component, const char *message);

/**
 * Get the currently configured log level.
 * @return One of the log levels defined in syslog.h.
 */
SO_PUBLIC extern int rzb_get_log_level(void);

/**
 * Get the currently configured log destination.
 * @return The current log distination.
 */
SO_PUBLIC extern RZB_LOG_DEST_t rzb_get_log_dest(void);

/**
 * Set logging to debug mode.
 * @return No return value.
 */
SO_PUBLIC extern void rzb_debug_logging(void);
#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_LOG_H
