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

/** @file thread_pool.h
 * Thread worker pool API.
 */

#ifndef RAZORBACK_THREAD_POOL_H
#define RAZORBACK_THREAD_POOL_H

#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/api.h>
#include <razorback/list.h>
#include <razorback/thread.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * Thread pool item
 */
struct ThreadPoolItem
{
    Thread_t *thread;         ///< The worker thread.
    int id;                   ///< The worker ID
    ThreadPool_t *pool;                 ///< The pool the worker belongs to.
};

/**
 * Create a ThreadPool.
 * @param initialThreads Number of threads to launch initially.
 * @param maxThreads The maximum number of workers allowed in the pool.
 * @param context The context of the worker threads.
 * @param namePattern The printf pattern for the worker thread names, must contain %d.
 * @param mainFunction The main routine for the threads.
 * @return A new ThreadPool or NULL on error.
 */
SO_PUBLIC extern ThreadPool_t * ThreadPool_Create(
    int initialThreads,
    int maxThreads,
    struct RazorbackContext *context,
    const char * namePattern,
    void (*mainFunction)(Thread_t *)
);

/**
 * Launch a worker.
 * @param pool The ThreadPool to spawn a worker in.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool ThreadPool_LaunchWorker(ThreadPool_t *pool);

/**
 * Launch several workers.
 * @param pool The ThreadPool to spawn the workers in.
 * @param count The number of workers to spawn.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool ThreadPool_LaunchWorkers(ThreadPool_t *pool, int count);

/**
 * Kill a worker.
 * @param pool The ThreadPool to which the worker belongs.
 * @param id The workers id.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool ThreadPool_KillWorker(ThreadPool_t *pool, int id);

/**
 * Kill all workers.
 * @param pool The ThreadPool to kill the workers in.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool ThreadPool_KillWorkers(ThreadPool_t *pool);

/**
 * Destroy a thread pool and release its resources.
 * @param pool The ThreadPool to destroy.
 * @return No return value.
 */
SO_PUBLIC extern void ThreadPool_Destroy(ThreadPool_t *pool);

/**
 * Get the number of live workers in the pool.
 * @param pool The ThreadPool to inspect.
 * @return The number of live workers.
 */
SO_PUBLIC extern size_t ThreadPool_GetAliveCount(ThreadPool_t *pool);

/**
 * Visit each live worker thread in the pool.
 * @param pool The ThreadPool to inspect.
 * @param function Callback invoked for each live worker thread.
 * @param userData Caller-provided callback state.
 * @return LIST_EACH_OK on success, or the callback return value that stopped iteration.
 */
SO_PUBLIC extern int ThreadPool_ForEach(
    ThreadPool_t *pool,
    int (*function)(Thread_t *thread, void *userData),
    void *userData
);

#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_THREAD_POOL_H
