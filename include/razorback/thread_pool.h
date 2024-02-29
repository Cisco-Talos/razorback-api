/** @file thread.h
 * Threading API.
 */

#ifndef	RAZORBACK_THREAD_POOL_H
#define	RAZORBACK_THREAD_POOL_H

#include <razorback/types.h>
#include <razorback/api.h>
#include <razorback/list.h>
#include <pthread.h>

/**
 * Thread pool item
 */
struct ThreadPoolItem
{
    struct Thread *thread;
    int id;
};

/**
 * Thread pool container.
 */
struct ThreadPool
{
    size_t limit;                              ///< Maximum number of threads
    int nextId;                             ///< Id of the next thread
    struct RazorbackContext *context;       ///< Context to spawn threads in
    void (*mainFunction) (struct Thread *); ///< Main function for spawned threads
    const char *namePattern;                      ///< Name pattern for threads
    struct List *list;
};

extern struct ThreadPool *
ThreadPool_Create(int initialThreads, int maxThreads, struct RazorbackContext *context, const char* namePattern, void (*mainFunction) (struct Thread *));

extern bool
ThreadPool_LaunchWorker(struct ThreadPool *pool);
extern bool
ThreadPool_LaunchWorkers(struct ThreadPool *pool, int count);

extern bool
ThreadPool_KillWorker(struct ThreadPool *pool, int id);
extern bool
ThreadPool_KillWorkers(struct ThreadPool *pool);

#endif // RAZORBACK_THREAD_POOL_H

