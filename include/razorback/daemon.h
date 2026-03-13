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

/** @file daemon.h
 * Razorback deamon helper.
 */
#ifndef __RAZORBACK_DAEMON_H__
#define __RAZORBACK_DAEMON_H__

#include <razorback/visibility.h>
#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Daemonize the application.
 * @param sighandler Signal handler callback.
 * @param pidFile PID file path.
 * @return true on successful daemonisation false on error.
 */
SO_PUBLIC extern bool rzb_daemonize(void (*sighandler)(int), const char *pidFile);
#ifdef __cplusplus
}
#endif
#endif /* __RAZORBACK_DAEMON_H__ */
