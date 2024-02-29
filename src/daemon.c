#include "config.h"

#include <signal.h>
#include <stdlib.h>
#include <razorback/daemon.h>
#include <razorback/log.h>

SO_PUBLIC bool
rzb_daemonize (void (*signal_handler) (int))
{
    pid_t pid, sid;

    if (rzb_get_log_dest () == RZB_LOG_DEST_ERR)
    {
        rzb_log (LOG_EMERG, "%s: Can't daemonize when using stderr for logging", __func__);
        return false;
    }

    if (signal_handler != NULL)
    {
        rzb_log (LOG_DEBUG, "%s: Installing new signal handler", __func__);
        signal (SIGHUP, signal_handler);
        signal (SIGTERM, signal_handler);
        signal (SIGINT, signal_handler);
        signal (SIGQUIT, signal_handler);
    }

    pid = fork ();
    if (pid < 0)
    {
        rzb_log (LOG_EMERG, "%s: Failed to daemonize", __func__);
        return false;
    }
    /* If we got a good PID, then
       we can exit the parent process. */
    if (pid > 0)
    {
        exit (EXIT_SUCCESS);
    }

    /* Create a new SID for the child process */
    sid = setsid ();
    if (sid < 0)
    {
        rzb_log (LOG_EMERG, "%s: Failed to become session leader", __func__);
        return false;
    }

    /* Close out the standard file descriptors */
    close (STDIN_FILENO);
    close (STDOUT_FILENO);
    close (STDERR_FILENO);
    return true;
}
