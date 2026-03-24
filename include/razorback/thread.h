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

#ifndef RAZORBACK_THREAD_H
#define RAZORBACK_THREAD_H

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
 * Purpose: hold the information about a thread
 */

/**
 * Launch a new thread.
 * @param function The function the thread will execute.
 * @param userData The thread user data.
 * @param name The name of the thread.
 * @param context The initial context of the thread.
 * @return Null on error a new Thread on success.
 */
SO_PUBLIC extern Thread_t * Thread_Launch(
    void (*function)(Thread_t *),
    void *userData,
    const char *name,
    struct RazorbackContext *context
);

/**
 * Change the registered context for a thread.
 * @param thread the thread to change.
 * @param context the new context.
 * @return The old context.
 */
SO_PUBLIC extern struct RazorbackContext * Thread_ChangeContext(
    Thread_t *thread,
    struct RazorbackContext *context
);

/**
 * Get the registered context for a thread.
 * @param p_pThread the thread to change.
 * @return The current context for the thread.
 */
SO_PUBLIC extern struct RazorbackContext * Thread_GetContext(Thread_t *p_pThread);

/**
 * Get the registered context for the current thread.
 * @return The current context.
 */
SO_PUBLIC extern struct RazorbackContext * Thread_GetCurrentContext(void);

/**
 * Destroy a thread wrapper.
 * @param thread The thread to destroy.
 * @return No return value.
 */
SO_PUBLIC extern void Thread_Destroy(Thread_t *thread);

/**
 * Check whether a thread is running.
 * @param thread The thread the test.
 * @return true if running, false if not.
 */
SO_PUBLIC extern bool Thread_IsRunning(Thread_t *thread);

/**
 * Check whether a thread has stopped.
 * @param thread The thread to test.
 * @return true if the thread has exited, false if its still running.
 */
SO_PUBLIC extern bool Thread_IsStopped(Thread_t *thread);

/**
 * Wait for a thread to terminate.
 * @param thread The target thread.
 * @return No return value.
 */
SO_PUBLIC extern void Thread_Join(Thread_t *thread);

/**
 * Interrupt a thread.
 * @param thread The target thread.
 * @return No return value.
 */
SO_PUBLIC extern void Thread_Interrupt(Thread_t *thread);

/**
 * Request that a thread stop.
 * @param thread The target thread.
 * @return No return value.
 */
SO_PUBLIC extern void Thread_Stop(Thread_t *thread);

/**
 * Request that a thread stop and wait for it to terminate.
 * @param thread Thread to operate on.
 * @return No return value.
 */
SO_PUBLIC extern void Thread_StopAndJoin(Thread_t *thread);

/**
 * Interrupt a thread and wait for it to terminate.
 * @param thread Thread to operate on.
 * @return No return value.
 */
SO_PUBLIC extern void Thread_InterruptAndJoin(Thread_t *thread);

/**
 * Yield execution from the current thread.
 * @return No return value.
 */
SO_PUBLIC extern void Thread_Yield(void);

/**
 * Sleep the current thread.
 * @param milliseconds Sleep duration in milliseconds.
 * @return No return value.
 */
SO_PUBLIC extern void Thread_Sleep(uint32_t milliseconds);

/**
 * Get the number of tracked threads.
 * @return the number of currently running threads.
 */
SO_PUBLIC extern uint32_t Thread_getCount(void);

/**
 * Get the configured global tracked-thread limit.
 * @return The maximum number of tracked threads allowed.
 */
SO_PUBLIC extern uint32_t Thread_getLimit(void);

/**
 * Get the current thread wrapper.
 * @return Current thread wrapper, or NULL if no wrapper is registered.
 */
SO_PUBLIC extern Thread_t * Thread_GetCurrent(void);

/**
 * Get the current thread identifier.
 * @return Current thread identifier.
 */
SO_PUBLIC extern rzb_thread_t Thread_GetCurrentId(void);

/**
 * Compare a thread entry against a key.
 * @param a A value.
 * @param id Identifier value.
 * @return Comparison result.
 */
SO_PUBLIC extern int Thread_KeyCmp(void *a, const void *id);

/**
 * Compare two thread entries.
 * @param a A value.
 * @param b B value.
 * @return Comparison result.
 */
SO_PUBLIC extern int Thread_Cmp(void *a, void *b);

/**
 * Set user data for a thread.
 * @param thread Thread to operate on.
 * @param data Data value.
 * @return No return value.
 */
SO_PUBLIC extern void Thread_SetUserData(Thread_t *thread, void *data);

/**
 * Get user data for a thread.
 * @param thread Thread to operate on.
 * @return Stored user data pointer, or NULL if no user data is set.
 */
SO_PUBLIC extern void * Thread_GetUserData(Thread_t *thread);

/**
 * Get the name assigned to a thread.
 * @param thread Thread to operate on.
 * @return Thread name string, or NULL if no name is set.
 */
SO_PUBLIC extern const char * Thread_GetName(Thread_t *thread);

#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_THREAD_H
