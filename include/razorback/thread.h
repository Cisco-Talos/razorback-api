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

/** @file thread.h
 * Threading API.
 */

#ifndef	RAZORBACK_THREAD_H
#define	RAZORBACK_THREAD_H

#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/api.h>
#ifdef _MSC_VER
typedef DWORD rzb_thread_t;
#else //_MSC_VER
#include <pthread.h>
typedef pthread_t rzb_thread_t;
#endif //_MSC_VER

#ifdef __cplusplus
extern "C" {
#endif

/** Thread
 * Purpose:	hold the information about a thread
 */

/** Create a new thread
 * @param *function The function the thread will execute
 * @param *userData The thread user data
 * @param *name The name of the thread
 * @param *context The initial context of the thread
 * @return Null on error a new Thread on success.
 */
SO_PUBLIC extern Thread_t *Thread_Launch (void (*function) (Thread_t *),
                                     void *userData, char *name,
                                     struct RazorbackContext *context);

/** Change the registered context of a running thread.
 * @param thread the thread to change
 * @param context the new context
 * @return The old context
 */
SO_PUBLIC extern struct RazorbackContext * Thread_ChangeContext(Thread_t *thread,
                                    struct RazorbackContext *context);

/** Get the registered context of a running thread.
 * @param thread the thread to change
 * @return The current context for the thread.
 */
SO_PUBLIC extern struct RazorbackContext * Thread_GetContext(Thread_t *p_pThread);

/** Get the running context for the current thread.
 * @return The current context.
 */
SO_PUBLIC extern struct RazorbackContext * Thread_GetCurrentContext(void);

/** Destroy a threads data
 * @param *thread The thread to destroy
 */
SO_PUBLIC extern void Thread_Destroy (Thread_t *thread);

/** Checks whether a thread is running or not
 * @param *thread The thread the test
 * @return true if running, false if not
 */
SO_PUBLIC extern bool Thread_IsRunning (Thread_t *thread);

/** Check if a thread has finished.
 * @param thread The thread to test.
 * @return true if the thread has exited, false if its still running.
 */
SO_PUBLIC extern bool Thread_IsStopped (Thread_t *thread);

/** Suspend execution of the calling thread until the target thread therminates.
 * @param thread The target thread.
 */
SO_PUBLIC extern void Thread_Join(Thread_t *thread);
/** Interrupt the target thread.
 * @param thread The target thread.
 */
SO_PUBLIC extern void Thread_Interrupt(Thread_t *thread);
/** Stop the target thread.
 * @param thread The target thread.
 */
SO_PUBLIC extern void Thread_Stop (Thread_t *thread);
SO_PUBLIC extern void Thread_StopAndJoin (Thread_t *thread);
SO_PUBLIC extern void Thread_InterruptAndJoin (Thread_t *thread);
/** Cause the current thread to yield execution to other runnable threads.
 */
SO_PUBLIC extern void Thread_Yield(void);

/** Get the number of currently running threads.
 * @return the number of currently running threads.
 */
SO_PUBLIC extern uint32_t Thread_getCount (void);

/** Get the current thread.
 */
SO_PUBLIC extern Thread_t *Thread_GetCurrent(void);
/** Get the current thread ID.
 */
SO_PUBLIC extern rzb_thread_t Thread_GetCurrentId(void);

SO_PUBLIC extern int Thread_KeyCmp(void *a, void *id);
SO_PUBLIC extern int Thread_Cmp(void *a, void *b);

SO_PUBLIC extern void Thread_SetUserData(Thread_t *thread, void *data);
SO_PUBLIC extern void *Thread_GetUserData(Thread_t *thread);

SO_PUBLIC extern char *Thread_GetName(Thread_t *thread);

#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_THREAD_H
