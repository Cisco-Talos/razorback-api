/** @file thread.h
 * Threading API.
 */

#ifndef	RAZORBACK_THREAD_POOL_H
#define	RAZORBACK_THREAD_POOL_H

#include <razorback/types.h>
#include <razorback/api.h>
#include <pthread.h>

/**
 * Thread pool item
 */
struct ThreadPoolItem
{
    struct Thread *thread;
    struct ThreadPoolItem *next;
    int id;
};

/**
 * Thread pool container.
 */
struct ThreadPool
{
    pthread_mutex_t mutex;                  ///< mutex protecting this struct
    int count;                              ///< Number of running threads
    int limit;                              ///< Maximum number of threads
    int nextId;                             ///< Id of the next thread
    struct RazorbackContext *context;       ///< Context to spawn threads in
    void (*mainFunction) (struct Thread *); ///< Main function for spawned threads
    struct ThreadPoolItem *head;            ///< Head of the thread list.
    const char *namePattern;                      ///< Name pattern for threads
};

struct ThreadPool *
ThreadPool_Create(int initialThreads, int maxThreads, struct RazorbackContext *context, const char* namePattern, void (*mainFunction) (struct Thread *));

bool
ThreadPool_LaunchWorker(struct ThreadPool *pool);

#endif // RAZORBACK_THREAD_POOL_H

