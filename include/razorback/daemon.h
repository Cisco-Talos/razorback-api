/** @file daemon.h
 * Razorback deamon helper.
 */
#ifndef __RAZORBACK_DAEMON_H__
#define __RAZORBACK_DAEMON_H__

#include <razorback/types.h>

/** Daemonize the application.
 * @return true on successful daemonisation false on error.
 */
extern bool rzb_daemonize (void (*sighandler) (int));

#endif /* __RAZORBACK_DAEMON_H__ */
