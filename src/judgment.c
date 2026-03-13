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
#include <razorback/block_id.h>
#include <razorback/ntlv.h>
#include <razorback/list.h>
#include <razorback/thread.h>
#include <razorback/judgment.h>
#include <razorback/debug.h>

#include <time.h>
#include <string.h>

#ifdef _MSC_VER
#include "bobins.h"
#endif


SO_PUBLIC struct Judgment *
Judgment_Create(struct EventId *eventId, struct BlockId *blockId) {
    struct Judgment *judgment = NULL;
    struct timespec l_tsTime;
    struct RazorbackContext *l_pContext;
    ASSERT(eventId != NULL);
    if (eventId == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: eventId is NULL", __func__);
        return NULL;
    }
    ASSERT(blockId != NULL);
    if (blockId == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: blockId is NULL", __func__);
        return NULL;
    }

    l_pContext = Thread_GetCurrentContext();

    memset(&l_tsTime, 0, sizeof(struct timespec));
    if (clock_gettime(CLOCK_REALTIME, &l_tsTime) == -1) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to get time stamp", __func__);
        return NULL;
    }

    if ((judgment = calloc(1, sizeof(struct Judgment))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed allocate judgment", __func__);
        return NULL;
    }

    if ((judgment->pMetaDataList = NTLVList_Create()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate judgment meta data", __func__);
        Judgment_Destroy(judgment);
        return NULL;
    }
    if (eventId != NULL) {
        if ((judgment->pEventId = EventId_Clone(eventId)) == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate judgment eventId", __func__);
            Judgment_Destroy(judgment);
            return NULL;
        }
    }
    if (blockId != NULL) {
        if ((judgment->pBlockId = BlockId_Clone(blockId)) == NULL) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate judgment blockId", __func__);
            Judgment_Destroy(judgment);
            return NULL;
        }
    }
    judgment->iSeconds = l_tsTime.tv_sec;
    judgment->iNanoSecs = l_tsTime.tv_nsec;
    uuid_copy(judgment->uuidNuggetId, l_pContext->uuidNuggetId);
    return judgment;
}

SO_PUBLIC void
Judgment_Destroy(struct Judgment *judgment) {
    ASSERT(judgment != NULL);
    if (judgment == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: judgment is NULL", __func__);
        return;
    }
    if (judgment->pEventId != NULL) {
        EventId_Destroy(judgment->pEventId);
    }

    if (judgment->pBlockId != NULL) {
        BlockId_Destroy(judgment->pBlockId);
    }

    if (judgment->pMetaDataList != NULL) {
        List_Destroy(judgment->pMetaDataList);
    }
    if (judgment->sMessage != NULL) {
        free(judgment->sMessage);
    }

    free(judgment);
}

