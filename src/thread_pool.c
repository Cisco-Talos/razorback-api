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

#include <razorback/thread_pool.h>
#include <razorback/thread.h>
#include <razorback/log.h>

#ifdef _MSC_VER
#include "bobins.h"
#endif

#define THREAD_POOL_KILL_WAIT_SLEEP_MS 10


static int
TP_KeyCmp(void *a, const void *id)
{
    struct ThreadPoolItem *item = (struct ThreadPoolItem *)a;
    const int *key = (const int *)id;
    if (*key == item->id)
        return 0;

    return -1;
}

static int
TP_Cmp(void *a, void *b)
{
    struct ThreadPoolItem *iA = (struct ThreadPoolItem *)a;
    struct ThreadPoolItem *iB = (struct ThreadPoolItem *)b;
    if (a == b)
        return 0;
    if (iA->id == iB->id)
        return 0;

    return -1;
}

static void
TP_Destroy(void *a)
{
    struct ThreadPoolItem *worker = (struct ThreadPoolItem *)a;
    if (worker->thread != NULL)
        Thread_Destroy(worker->thread);
    free(a);
}

static void
ThreadPool_Main(Thread_t *thread)
{
    struct ThreadPoolItem *worker = Thread_GetUserData(thread);
    struct ThreadPool *pool = worker->pool;
    Thread_SetUserData(thread,NULL);

    pool->mainFunction(thread);

    List_Remove(pool->list, worker);
}

SO_PUBLIC struct ThreadPool *
ThreadPool_Create(int initialThreads,
        int maxThreads,
        struct RazorbackContext *context,
        const char *namePattern,
        void (*mainFunction) (Thread_t *))
{
    struct ThreadPool *pool;
    if ((pool = (struct ThreadPool *)calloc(1,sizeof(struct ThreadPool))) == NULL)
        return NULL;
    if ((pool->list = List_Create( LIST_MODE_GENERIC,
                    TP_Cmp,
                    TP_KeyCmp,
                    TP_Destroy,
                    NULL,
                    NULL,
                    NULL)) == NULL)
    {
        free(pool);
        return NULL;
    }

    List_SetLimit(pool->list, maxThreads);
    pool->limit = maxThreads;
    pool->context = context;
    pool->mainFunction = mainFunction;
    pool->namePattern = namePattern;

    if (!ThreadPool_LaunchWorkers(pool, initialThreads))
    {
        ThreadPool_Destroy(pool);
        return NULL;
    }

    return pool;
}


SO_PUBLIC bool
ThreadPool_LaunchWorker(struct ThreadPool *pool)
{
    struct ThreadPoolItem *item;
    char* name;


    if (( item = (struct ThreadPoolItem *)calloc(1,sizeof(struct ThreadPoolItem))) == NULL)
    {
        return false;
    }
    item->id = pool->nextId++;
    item->pool = pool;

    if (asprintf(&name, pool->namePattern, item->id) == -1)
    {
        free(item);
        return false;
    }
    if (!List_Push(pool->list, item)) {
        free(item);
        free(name);
        return false;
    }

    item->thread = Thread_Launch(ThreadPool_Main, item, name, pool->context);
    if (item->thread == NULL) {
        List_Remove(pool->list, item);
        free(name);
        return false;
    }
    return true;
}

SO_PUBLIC bool
ThreadPool_LaunchWorkers(struct ThreadPool *pool, int count)
{
    int i=0;
    for (i = 0; i < count; i++)
    {
        if (!ThreadPool_LaunchWorker(pool))
        {
            return false;
        }
    }
    return true;
}

SO_PUBLIC bool
ThreadPool_KillWorker(struct ThreadPool *pool, int id)
{
    struct ThreadPoolItem *worker;
    worker = (struct ThreadPoolItem *)List_Find(pool->list, &id);

    if (worker == NULL)
        return false;

    Thread_InterruptAndJoin(worker->thread);
    return true;
}

static int
ThreadPool_Kill(void *vItem, void *userData)
{
    struct ThreadPoolItem *item = (struct ThreadPoolItem *)vItem;
    (void)userData;

    Thread_Interrupt(item->thread);
    return LIST_EACH_OK;
}


SO_PUBLIC bool
ThreadPool_KillWorkers(struct ThreadPool *pool)
{
    size_t count;

    if (pool == NULL || pool->list == NULL)
        return false;

    List_ForEach(pool->list, ThreadPool_Kill, NULL);

    for (count = List_Length(pool->list); count > 0; count = List_Length(pool->list))
        Thread_Sleep(THREAD_POOL_KILL_WAIT_SLEEP_MS);

    return true;
}

SO_PUBLIC void
ThreadPool_Destroy(struct ThreadPool *pool)
{
    if (pool == NULL)
        return;

    if (pool->list != NULL) {
        if (List_Length(pool->list) > 0)
            ThreadPool_KillWorkers(pool);

        List_Destroy(pool->list);
        pool->list = NULL;
    }

    free(pool);
}

SO_PUBLIC size_t
ThreadPool_GetAliveCount(struct ThreadPool *pool)
{
    if (pool == NULL || pool->list == NULL)
        return 0;

    return List_Length(pool->list);
}
