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
#include <razorback/debug.h>
#include <signal.h>
#include <stdlib.h>
#ifndef _MSC_VER
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <razorback/daemon.h>
#include <razorback/log.h>
#ifdef _MSC_VER
static bool rzb_daemonize_win32(void);
#else //_MSC_VER
static bool rzb_daemonize_posix(void (*signal_handler)(int), const char *pidFile);
#endif

SO_PUBLIC bool
rzb_daemonize (void (*signal_handler) (int), const char *pidFile) {
#ifdef _MSC_VER
    return rzb_daemonize_win32();
#else //_MSC_VER
    return rzb_daemonize_posix(signal_handler, pidFile);
#endif
}



#ifdef _MSC_VER
static bool rzb_daemonize_win32(void)
{
    UNIMPLEMENTED();
    return true;
}
#else //_MSC_VER
static const char *sg_pidFile = NULL;

static void
unlinkPidFile(void) {
    if (sg_pidFile != NULL)
        unlink(sg_pidFile);
}
bool
rzb_daemonize_posix (void (*signal_handler) (int), const char *pidFile)
{
    pid_t pid, sid;

    if (rzb_get_log_dest() == RZB_LOG_DEST_ERR) {
        rzb_log(LOG_EMERG, LOG_C_CORE, "%s: Can't daemonize when using stderr for logging", __func__);
        return false;
    }

    if (signal_handler != NULL) {
        rzb_log(LOG_DEBUG, LOG_C_CORE, "%s: Installing new signal handler", __func__);
        signal(SIGHUP, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGINT, signal_handler);
        signal(SIGQUIT, signal_handler);
    }

    pid = fork();
    if (pid < 0) {
        rzb_log(LOG_EMERG, LOG_C_CORE, "%s: Failed to daemonize", __func__);
        return false;
    }
    /* If we got a good PID, then
       we can exit the parent process. */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        rzb_log(LOG_EMERG, LOG_C_CORE, "%s: Failed to become session leader", __func__);
        return false;
    }

    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    if (pidFile != NULL) {
        /* save the PID */
        pid_t mainpid = getpid();
        FILE *fd;
        mode_t old_umask = umask(0002);
        if ((fd = fopen(pidFile, "w")) == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "Can't save PID in file %s", pidFile);
        } else {
            if (fprintf(fd, "%u", (unsigned int) mainpid) < 0) {
                rzb_log(LOG_ERR, LOG_C_CORE, "Can't save PID in file %s", pidFile);
            }
            fclose(fd);
        }
        umask(old_umask);
        sg_pidFile = pidFile;
        atexit(unlinkPidFile);
    }

    return true;
}


#endif

