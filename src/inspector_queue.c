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
#include <razorback/inspector_queue.h>
#include <razorback/queue.h>
#include <razorback/queue_list.h>
#include <razorback/block.h>
#include <razorback/log.h>
#include <razorback/event.h>
#include <razorback/api.h>
#include <errno.h>

/** Globals
*/
static List_t *sg_qlInspectorQueue;
static bool sg_bInspectorIntialized = false;

static void
InspectorQueue_GetQueueName (uuid_t p_pApplicationType,
                             char * p_sQueueName,
                             size_t p_iQueueNameSize)
{
    Queue_GetQueueName ( "INSPECTOR",
                        p_pApplicationType, p_sQueueName, p_iQueueNameSize);
}

SO_PUBLIC struct Queue *
InspectorQueue_Initialize (uuid_t p_pApplicationType, int p_iFlags)
{

    // the name
    char l_sQueueName[128];
    // the queue from the list
    struct Queue *l_pQueue;

    // setup the global variables
    if (!sg_bInspectorIntialized)
    {
        sg_qlInspectorQueue = QueueList_Create();
        sg_bInspectorIntialized = true;
    }

    // transform to correct name
    InspectorQueue_GetQueueName (p_pApplicationType, l_sQueueName, sizeof(l_sQueueName));

    // does this queue already exist?
    // if so, done
    l_pQueue = QueueList_Find (sg_qlInspectorQueue, p_pApplicationType);
    if (l_pQueue != NULL)
        return l_pQueue;

    // initialize the queue
    if ((l_pQueue = Queue_Create (l_sQueueName, false, p_iFlags)) == NULL)
        return NULL;

    // find the queue
    if (!QueueList_Add (sg_qlInspectorQueue, l_pQueue, p_pApplicationType))
    {
        Queue_Terminate(l_pQueue);
        return NULL;
    }


    return l_pQueue;
}

SO_PUBLIC void
InspectorQueue_Terminate (uuid_t p_pApplicationType)
{
    // the queue from the list
    struct Queue *l_pQueue;

    // if we never initialized, do nothing
    if (!sg_bInspectorIntialized)
        return;

    // find the queue and terminate it
    l_pQueue = QueueList_Find (sg_qlInspectorQueue, p_pApplicationType);
    if (l_pQueue == NULL)
        return;

    Queue_Terminate (l_pQueue);
    QueueList_Remove(sg_qlInspectorQueue, p_pApplicationType);
}
