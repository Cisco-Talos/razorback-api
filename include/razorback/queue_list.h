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

/** @file queue_list.h
 * A list of queues.
 */
#ifndef RAZORBACK_QUEUE_LIST_H
#define RAZORBACK_QUEUE_LIST_H
#include <razorback/visibility.h>
#include <razorback/queue.h>
#include <razorback/list.h>

#ifdef __cplusplus
extern "C" {
#endif

/** QueueListEntry
 * an entry in a list of queues
 */
struct QueueListEntry
{
    struct Queue *pQueue;          ///< the queue
    uuid_t uuiKey;                 ///< a key for the queue
    struct QueueListEntry *pNext;  ///< the next item in the list
};

/**
 * Create a queue list.
 * @return Requested object on success, or NULL on failure.
 */
SO_PUBLIC extern List_t * QueueList_Create(void);

/**
 * finds a queue in a list.
 * @param p_pList the list.
 * @param p_pId the id of the queue to find.
 * @return a pointer to the queue or null if not found.
 */
SO_PUBLIC extern struct Queue * QueueList_Find(List_t *p_pList, const uuid_t p_pId);

/**
 * adds a queue to a list.
 * @param p_pList the list.
 * @param p_pQ Q object.
 * @param p_pId the id of the queue to add.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool QueueList_Add(List_t *p_pList, struct Queue *p_pQ, const uuid_t p_pId);

/**
 * Remove a queue to a list.
 * @param p_pList the list.
 * @param p_pId the id of the queue to add.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool QueueList_Remove(List_t *p_pList, const uuid_t p_pId);

/**
 * Get the first queue list entry.
 * @param p_pList the list.
 * @return Requested object on success, or NULL on failure.
 */
SO_PUBLIC extern struct QueueListEntry * QueueList_First(const List_t *p_pList);

/**
 * Get the next queue list entry.
 * @param p_pList the list.
 * @param p_pCurrent the current entry.
 * @return Requested object on success, or NULL on failure.
 */
SO_PUBLIC extern struct QueueListEntry * QueueList_Next(
    const List_t *p_pList,
    const struct QueueListEntry *p_pCurrent
);
#ifdef __cplusplus
}
#endif
#endif
