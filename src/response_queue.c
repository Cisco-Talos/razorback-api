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
#include <razorback/debug.h>
#include <razorback/response_queue.h>
#include <razorback/queue.h>
#include <razorback/queue_list.h>
#include <razorback/block_id.h>
#include <razorback/log.h>
#include <razorback/api.h>
#include <errno.h>

/** Globals
*/
static List_t *sg_qlResponseQueue;
static bool sg_bResponseInitialized = false;

static void
ResponseQueue_GetQueueName (uuid_t p_pCollectorId, char * p_sQueueName,
                            size_t p_iQueueNameSize)
{
    Queue_GetQueueName ("RESPONSE", p_pCollectorId,
                        p_sQueueName, p_iQueueNameSize);
}

SO_PUBLIC struct Queue *
ResponseQueue_Initialize (uuid_t p_pCollectorId, int p_iFlags)
{
    // the name
    char l_sQueueName[128];
    // the queue from the list
    struct Queue *l_pQueue;

    // setup the global variables
    if (!sg_bResponseInitialized)
    {
        sg_qlResponseQueue = QueueList_Create();
        sg_bResponseInitialized = true;
    }

    // transform to correct name
    ResponseQueue_GetQueueName (p_pCollectorId, l_sQueueName, sizeof(l_sQueueName));

    // does this queue already exist?
    // if so, done
    l_pQueue = QueueList_Find (sg_qlResponseQueue, p_pCollectorId);
    if (l_pQueue != NULL)
        return l_pQueue;

    // initialize the queue
    if ((l_pQueue = Queue_Create (l_sQueueName, false, p_iFlags)) == NULL)
    {
        rzb_log (LOG_ERR, LOG_C_QUEUE,
                 "%s: failed due to failure of Queue_Initialize", __func__);
        return NULL;
    }

    // find the queue
    if (!QueueList_Add (sg_qlResponseQueue, l_pQueue, p_pCollectorId))
    {
        rzb_log (LOG_ERR, LOG_C_QUEUE,
                 "%s: failed due to failure of QueueList_Add", __func__);
        Queue_Terminate(l_pQueue);
        return NULL;
    }


    // done
    return l_pQueue;
}

SO_PUBLIC void
ResponseQueue_Terminate (uuid_t p_pCollectorId)
{
    // the queue from the list
    struct Queue *l_pQueue;

    // if never initialized, do nothing
    if (!sg_bResponseInitialized)
        return;

    // find the queue and terminate it
    l_pQueue = QueueList_Find (sg_qlResponseQueue, p_pCollectorId);
    if (l_pQueue == NULL)
        return;
    QueueList_Remove(sg_qlResponseQueue, p_pCollectorId);
    Queue_Terminate (l_pQueue);
}
