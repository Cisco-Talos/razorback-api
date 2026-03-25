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
#include <razorback/types.h>
#include <razorback/debug.h>
#include <razorback/lock.h>
#include <razorback/log.h>
#include <errno.h>

#ifdef _MSC_VER
#else //_MSC_VER
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#endif //_MSC_VER


/** Mutex Control Structure
 */
struct _Mutex {
#ifdef _MSC_VER
    HANDLE recursiveLock;  ///< Recursive lock handle (win32)
    CRITICAL_SECTION cs;   ///< None recursive lock handle (win32)
#else
    pthread_mutex_t lock;       ///< PThreads lock structure.
    pthread_mutexattr_t attrs;  ///< PThreads lock attributes.
#endif
    int mode;  ///< Lock mode
};

/** RWLock Control Structure
 */
struct _RWLock {
#ifdef _MSC_VER
    HANDLE recursiveLock;  ///< Recursive lock handle (win32)
    CRITICAL_SECTION cs;   ///< None recursive lock handle (win32)
#else
    pthread_rwlock_t lock;       ///< PThreads lock structure.
    pthread_rwlockattr_t attrs;  ///< PThreads lock attributes.
#endif
    int mode;  ///< Lock mode
};



/** Semaphore Control Structure
 */
struct _Semaphore {
#ifdef _MSC_VER
    HANDLE sem;  ///< Semaphore handle (win32)
#else
    sem_t sem;  ///< PThreads semaphore
#endif
};


static bool Mutex_Init(Mutex_t *);
static bool RWLock_Init(RWLock_t *);

SO_PUBLIC Mutex_t *
Mutex_Create(int mode) {
    Mutex_t *ret;
    if ((ret = (Mutex_t *)calloc(1,sizeof(Mutex_t))) == NULL) {
        return NULL;
    }
    ret->mode = mode;
    if (!Mutex_Init(ret)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Mutex_Init failed", __func__);
        Mutex_Destroy(ret);
        return NULL;
    }
    return ret;
}

SO_PUBLIC bool
Mutex_Lock(Mutex_t *mutex) {
    ASSERT(mutex != NULL);
    if (mutex == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: mutex is NULL", __func__);
        return false;
    }
#ifdef _MSC_VER
    switch (mutex->mode)
    {
    case MUTEX_MODE_RECURSIVE:
        WaitForSingleObject(mutex->recursiveLock, INFINITE);
        break;
    case MUTEX_MODE_NORMAL:
        EnterCriticalSection(&mutex->cs);
        break;
    default:
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Invalid mutex mode: %d", mutex->mode);
        return false;
    }
#else //_MSC_VER
    if (pthread_mutex_lock(&mutex->lock) != 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: pthread_mutex_lock failed", __func__);
        return false;
    }
#endif //_MSC_VER
    return true;
}

SO_PUBLIC bool
Mutex_Unlock(Mutex_t *mutex) {
    ASSERT(mutex != NULL);
    if (mutex == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: mutex is NULL", __func__);
        return false;
    }
#ifdef _MSC_VER
    switch (mutex->mode)
    {
    case MUTEX_MODE_RECURSIVE:
        ReleaseMutex(mutex->recursiveLock);
        break;
    case MUTEX_MODE_NORMAL:
        LeaveCriticalSection(&mutex->cs);
        break;
    default:
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Invalid mutex mode: %d", mutex->mode);
        return false;
    }
#else //_MSC_VER
    if (pthread_mutex_unlock(&mutex->lock) != 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: pthread_mutex_unlock failed", __func__);
        return false;
    }
#endif //_MSC_VER
    return true;
}

SO_PUBLIC void
Mutex_Destroy(Mutex_t *mutex) {
    ASSERT(mutex != NULL);
    if (mutex == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: mutex is NULL", __func__);
        return;
    }
#ifdef _MSC_VER
    switch (mutex->mode)
    {
    case MUTEX_MODE_RECURSIVE:
        CloseHandle(mutex->recursiveLock);
        break;
    case MUTEX_MODE_NORMAL:
        DeleteCriticalSection(&mutex->cs);
        break;
    default:
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Invalid mutex mode: %d", mutex->mode);
    }

#else //_MSC_VER
    pthread_mutex_destroy(&mutex->lock);
    pthread_mutexattr_destroy(&mutex->attrs);
#endif //_MSC_VER
    free(mutex);
}


SO_PUBLIC RWLock_t *
RWLock_Create() {
    RWLock_t *ret;
    if ((ret = calloc(1,sizeof(RWLock_t))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: calloc failed", __func__);
        return NULL;
    }

    if (!RWLock_Init(ret)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: RWLock_Init failed", __func__);
        RWLock_Destroy(ret);
        return NULL;
    }
    return ret;
}

SO_PUBLIC bool
RWLock_ReadLock(RWLock_t *rwlock) {
    ASSERT(rwlock != NULL);
    if (rwlock == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: rwlock is NULL", __func__);
        return false;
    }
#ifdef _MSC_VER
    switch (rwlock->mode)
    {
    case MUTEX_MODE_RECURSIVE:
        WaitForSingleObject(rwlock->recursiveLock, INFINITE);
        break;
    case MUTEX_MODE_NORMAL:
        EnterCriticalSection(&rwlock->cs);
        break;
    default:
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Invalid rwlock mode: %d", rwlock->mode);
        return false;
    }
#else //_MSC_VER
    if (pthread_rwlock_rdlock(&rwlock->lock) != 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: pthread_rwlock_rdlock failed", __func__);
        return false;
    }
#endif //_MSC_VER
    return true;
}

SO_PUBLIC bool
RWLock_WriteLock(RWLock_t *rwlock) {
    ASSERT(rwlock != NULL);
    if (rwlock == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: rwlock is NULL", __func__);
        return false;
    }
#ifdef _MSC_VER
    switch (rwlock->mode)
    {
    case MUTEX_MODE_RECURSIVE:
        WaitForSingleObject(rwlock->recursiveLock, INFINITE);
        break;
    case MUTEX_MODE_NORMAL:
        EnterCriticalSection(&rwlock->cs);
        break;
    default:
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Invalid rwlock mode: %d", rwlock->mode);
        return false;
    }
#else //_MSC_VER
    if (pthread_rwlock_wrlock(&rwlock->lock) != 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: pthread_rwlock_wrlock failed", __func__);
        return false;
    }
#endif //_MSC_VER
    return true;
}

SO_PUBLIC bool
RWLock_Unlock(RWLock_t *rwlock) {
    ASSERT(rwlock != NULL);
    if (rwlock == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: rwlock is NULL", __func__);
        return false;
    }
#ifdef _MSC_VER
    switch (rwlock->mode)
    {
    case MUTEX_MODE_RECURSIVE:
        ReleaseRWLock(rwlock->recursiveLock);
        break;
    case MUTEX_MODE_NORMAL:
        LeaveCriticalSection(&rwlock->cs);
        break;
    default:
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Invalid rwlock mode: %d", rwlock->mode);
        return false;
    }
#else //_MSC_VER
    if (pthread_rwlock_unlock(&rwlock->lock) != 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: pthread_rwlock_unlock failed", __func__);
        return false;
    }
#endif //_MSC_VER
    return true;
}

SO_PUBLIC void
RWLock_Destroy(RWLock_t *rwlock) {
    ASSERT(rwlock != NULL);
    if (rwlock == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: rwlock is NULL", __func__);
        return;
    }
#ifdef _MSC_VER
    switch (rwlock->mode)
    {
    case MUTEX_MODE_RECURSIVE:
        CloseHandle(rwlock->recursiveLock);
        break;
    case MUTEX_MODE_NORMAL:
        DeleteCriticalSection(&rwlock->cs);
        break;
    default:
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Invalid rwlock mode: %d", rwlock->mode);
    }

#else //_MSC_VER
    pthread_rwlock_destroy(&rwlock->lock);
    pthread_rwlockattr_destroy(&rwlock->attrs);
#endif //_MSC_VER
    free(rwlock);
}


SO_PUBLIC Semaphore_t *
Semaphore_Create(bool shared, unsigned int value) {
    Semaphore_t *ret;
    if (( ret = (Semaphore_t *)calloc(1,sizeof(Semaphore_t))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: calloc failed", __func__);
        return NULL;
    }
#ifdef _MSC_VER
    ret->sem = CreateSemaphore(NULL,0,LONG_MAX,NULL);
    if (!ret->sem)
    {
        free(ret);
        return NULL;
    }

#else //_MSC_VER
    sem_init (&ret->sem, (shared ? 1 : 0), value);
#endif //_MSC_VER
    return ret;
}

SO_PUBLIC bool
Semaphore_Post(Semaphore_t *sem) {
    ASSERT(sem != NULL);
    if (sem == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: sem is NULL", __func__);
        return false;
    }
#ifdef _MSC_VER
    ReleaseSemaphore(sem->sem, 1, NULL);
#else //_MSC_VER
    if (sem_post(&sem->sem) != 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: sem_post failed", __func__);
        return false;
    }
#endif //_MSC_VER
    return true;
}

SO_PUBLIC bool
Semaphore_TimedWait(Semaphore_t *sem, uint32_t timeoutMilliseconds) {
    ASSERT(sem != NULL);
    if (sem == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: sem is NULL", __func__);
        return false;
    }
#ifdef _MSC_VER
    if (timeoutMilliseconds == 0) {
        WaitForSingleObject(sem->sem, INFINITE);
        return true;
    }

    switch (WaitForSingleObject(sem->sem, timeoutMilliseconds))
    {
    case WAIT_OBJECT_0:
        return true;
    case WAIT_TIMEOUT:
        errno = EAGAIN;
        return false;
    default:
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: WaitForSingleObject failed", __func__);
        return false;
    }
#else //_MSC_VER
    if (timeoutMilliseconds == 0) {
        while (sem_wait(&sem->sem) != 0) {
            if (errno == EINTR)
                continue;
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: sem_wait failed", __func__);
            return false;
        }
        return true;
    }

    {
        struct timespec deadline;

        if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: clock_gettime failed", __func__);
            return false;
        }

        deadline.tv_sec += timeoutMilliseconds / 1000;
        deadline.tv_nsec += (long)(timeoutMilliseconds % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec += 1;
            deadline.tv_nsec -= 1000000000L;
        }

        while (sem_timedwait(&sem->sem, &deadline) != 0) {
            if (errno == EINTR)
                continue;
            if (errno == ETIMEDOUT) {
                errno = EAGAIN;
                return false;
            }
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: sem_timedwait failed", __func__);
            return false;
        }
    }
#endif //_MSC_VER
    return true;
}

SO_PUBLIC bool
Semaphore_Wait(Semaphore_t *sem) {
    ASSERT(sem != NULL);
    if (sem == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: sem is NULL", __func__);
        return false;
    }
#ifdef _MSC_VER
    WaitForSingleObject(sem->sem, INFINITE);
#else //_MSC_VER
    if (sem_wait(&sem->sem) != 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: sem_wait failed", __func__);
        return false;
    }
#endif //_MSC_VER
    return true;
}

SO_PUBLIC void
Semaphore_Destroy(Semaphore_t *sem) {
    ASSERT(sem != NULL);
    if (sem == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: sem is NULL", __func__);
        return;
    }
#ifdef _MSC_VER
    CloseHandle(sem->sem);
#else //_MSC_VER
    sem_destroy(&sem->sem);
#endif //_MSC_VER
    free(sem);
}



#ifdef _MSC_VER
static bool Mutex_Init(Mutex_t *mutex)
{
    ASSERT(mutex != NULL);
    if (mutex == NULL)
        return false;
    switch (mutex->mode)
    {
    case MUTEX_MODE_RECURSIVE:
        mutex->recursiveLock = CreateMutex(
            NULL,              // default security attributes
            FALSE,             // initially not owned
            NULL);              // unnamed mutex
        if (mutex->recursiveLock == NULL)
            return false;
        break;
    case MUTEX_MODE_NORMAL:
        InitializeCriticalSection(&mutex->cs);
        break;
    default:
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Invalid mutex mode: %d", mutex->mode);
        return false;
    }

    return true;
}
#else //_MSC_VER
static bool Mutex_Init(Mutex_t *mutex) {
    ASSERT(mutex != NULL);
    if (mutex == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: mutex is NULL", __func__);
        return false;
    }
    pthread_mutexattr_init(&mutex->attrs);

    switch (mutex->mode) {
        case MUTEX_MODE_RECURSIVE:
            pthread_mutexattr_settype(&mutex->attrs, PTHREAD_MUTEX_RECURSIVE);
            break;
        case MUTEX_MODE_NORMAL:
            break;
        default:
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Invalid mutex mode: %d", __func__, mutex->mode);
            return false;
    }
    pthread_mutex_init(&mutex->lock, &mutex->attrs);
    return true;
}

static bool RWLock_Init(RWLock_t *rwlock) {
    ASSERT(rwlock != NULL);
    if (rwlock == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: rwlock is NULL", __func__);
        return false;
    }
    pthread_rwlockattr_init(&rwlock->attrs);
    pthread_rwlock_init(&rwlock->lock, &rwlock->attrs);
    return true;
}
#endif //_MSC_VER
