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

/** @file lock.h
 * Locking functions.
 */
#ifndef RAZORBACK_LOCK_H
#define RAZORBACK_LOCK_H

#include <razorback/visibility.h>
#include <razorback/types.h>


#ifdef __cplusplus
extern "C" {
#endif

#define MUTEX_MODE_NORMAL       0   ///< None recursively lockable mutex
#define MUTEX_MODE_RECURSIVE    1   ///< Recursively lockable mutex

/**
 * Create a mutex.
 * @param mode One of MUTEX_MODE_NORMAL or MUTEX_MODE_RECURSIVE.
 * @return A new mutex structure or NULL on error.
 */
SO_PUBLIC extern Mutex_t * Mutex_Create(int mode);

/**
 * Lock a mutex.
 * @param mutex The mutex to lock.
 * @return true on success false on failure.
 */
SO_PUBLIC extern bool Mutex_Lock(Mutex_t *mutex);

/**
 * Unlock a mutex.
 * @param mutex The mutex to unlock.
 * @return true on success false on failure.
 */
SO_PUBLIC extern bool Mutex_Unlock(Mutex_t *mutex);

/**
 * Destroy a mutex.
 * @param mutex The mutex to destroy.
 * @return No return value.
 */
SO_PUBLIC extern void Mutex_Destroy(Mutex_t *mutex);

/**
 * Create a rwlock.
 * @return A new rwlock structure or NULL on error.
 */
SO_PUBLIC extern RWLock_t * RWLock_Create(void);

/**
 * Lock a rwlock.
 * @param rwlock The rwlock to lock.
 * @return true on success false on failure.
 */
SO_PUBLIC extern bool RWLock_ReadLock(RWLock_t *rwlock);

/**
 * Acquire a write lock on an RW lock.
 * @param rwlock RW lock to lock for writing.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool RWLock_WriteLock(RWLock_t *rwlock);

/**
 * Unlock a rwlock.
 * @param rwlock The rwlock to unlock.
 * @return true on success false on failure.
 */
SO_PUBLIC extern bool RWLock_Unlock(RWLock_t *rwlock);

/**
 * Destroy a rwlock.
 * @param rwlock The rwlock to destroy.
 * @return No return value.
 */
SO_PUBLIC extern void RWLock_Destroy(RWLock_t *rwlock);

/**
 * Create a semaphore.
 * @param shared True if share, false if not.
 * @param value Initial count.
 * @return A new semaphore structure or NULL on error.
 */
SO_PUBLIC extern Semaphore_t * Semaphore_Create(bool shared, unsigned int value);

/**
 * Post to a semaphore.
 * @param sem The semaphore to post to.
 * @return true on success false on failure.
 */
SO_PUBLIC extern bool Semaphore_Post(Semaphore_t *sem);

/**
 * Wait for a slot on the semaphore.
 * @param sem The semaphore to wait on.
 * @return true on success false on failure.
 */
SO_PUBLIC extern bool Semaphore_Wait(Semaphore_t *sem);

/**
 * Wait on a semaphore with a timeout.
 * @param sem Semaphore to wait on.
 * @param timeoutMilliseconds Timeout in milliseconds. A value of 0 waits indefinitely.
 * @return true on success, false on timeout or failure.
 */
SO_PUBLIC extern bool Semaphore_TimedWait(Semaphore_t *sem,
                                          uint32_t timeoutMilliseconds);

/**
 * Destroy a semaphore.
 * @param sem The semaphore to destroy.
 * @return No return value.
 */
SO_PUBLIC extern void Semaphore_Destroy(Semaphore_t *sem);

#ifdef __cplusplus
}
#endif
#endif //RAZORBACK_LOCK_H
