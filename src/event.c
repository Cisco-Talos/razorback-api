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

#include <razorback/event.h>
#include <razorback/log.h>
#include <razorback/block.h>
#include <razorback/ntlv.h>
#include <razorback/list.h>
#include <razorback/debug.h>

#include <time.h>
#include <string.h>

#ifdef _MSC_VER
#include "bobins.h"
#endif

SO_PUBLIC struct EventId *
EventId_Create (void) {
    struct EventId *id;
    struct timespec l_tsTime;

    memset(&l_tsTime, 0, sizeof(struct timespec));
    if (clock_gettime(CLOCK_REALTIME, &l_tsTime) == -1) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get time stamp", __func__);
        return NULL;
    }
    if ((id = calloc(1, sizeof(struct EventId))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed allocate event", __func__);
        return NULL;
    }

    id->iSeconds = l_tsTime.tv_sec;
    id->iNanoSecs = l_tsTime.tv_nsec;
    return id;
}

SO_PUBLIC struct EventId *
EventId_Clone (struct EventId *in) {
    struct EventId *id;

    ASSERT(in != NULL);
    if (in == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL input id", __func__);
        return NULL;
    }

    if ((id = calloc(1, sizeof(struct EventId))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed allocate event", __func__);
        return NULL;
    }
    uuid_copy(id->uuidNuggetId, in->uuidNuggetId); 
    id->iSeconds = in->iSeconds;
    id->iNanoSecs = in->iNanoSecs;
    return id;
}

SO_PUBLIC void
EventId_Destroy (struct EventId *id) {
    ASSERT(id != NULL);
    if (id == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL id", __func__);
        return;
    }
    free(id);
}

SO_PUBLIC struct Event *
Event_Create (void) {
    struct Event *event = NULL;

    if ((event = calloc(1, sizeof(struct Event))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed allocate event", __func__);
        return NULL;
    }
    if ((event->pId = EventId_Create()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate event id data", __func__);
        Event_Destroy(event);
        return NULL;
    }

    if ((event->pMetaDataList = NTLVList_Create()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate event meta data", __func__);
        Event_Destroy(event);
        return NULL;
    }

    if ((event->pBlock = Block_Create()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate event block data", __func__);
        Event_Destroy(event);
        return NULL;
    }

    return event;
}

SO_PUBLIC void
Event_Destroy (struct Event *event) {
    ASSERT(event != NULL);
    if (event == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: NULL event", __func__);
        return;
    }

    if (event->pId != NULL) {
        EventId_Destroy(event->pId);
    }

    if (event->pBlock != NULL) {
        Block_Destroy(event->pBlock);
    }

    if (event->pMetaDataList != NULL) {
        List_Destroy(event->pMetaDataList);
    }

    if (event->pParent != NULL) {
        Event_Destroy(event->pParent);
    }

    if (event->pParentId != NULL) {
        EventId_Destroy(event->pParentId);
    }

    free(event);
}
