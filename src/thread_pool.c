#include "config.h"

#include <razorback/thread_pool.h>
#include <razorback/thread.h>

SO_PUBLIC struct ThreadPool *
ThreadPool_Create(int initialThreads, 
        int maxThreads, 
        struct RazorbackContext *context, 
        const char *namePattern,
        void (*mainFunction) (struct Thread *))
{
    struct ThreadPool *pool;
    if ((pool = calloc(1,sizeof(struct ThreadPool))) == NULL)
        return NULL;

    pthread_mutex_init(&pool->mutex, NULL);
    pool->count = 0;
    pool->limit = maxThreads;
    pool->context = context;
    pool->mainFunction = mainFunction;
    pool->namePattern = namePattern;

    for (int i = 0; i < initialThreads; i++)
        ThreadPool_LaunchWorker(pool);

    return pool;
}


SO_PUBLIC bool
ThreadPool_LaunchWorker(struct ThreadPool *pool)
{
    struct ThreadPoolItem *item;
    char* name;

    if (( item = calloc(1,sizeof(struct ThreadPoolItem))) == NULL)
        return false;
    pthread_mutex_lock(&pool->mutex);
    item->id = pool->nextId++;
    if (asprintf(&name, pool->namePattern, item->id) == -1)
    {   
        pthread_mutex_unlock(&pool->mutex);
        free(item);
        return false;
    }
    item->thread = Thread_Launch(pool->mainFunction, NULL, name, pool->context);
    item->next = pool->head;
    pool->head = item;
    pthread_mutex_unlock(&pool->mutex);
    return true;
}

