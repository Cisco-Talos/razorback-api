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

#include "dev_mode.h"

#include <razorback/api.h>
#include <razorback/block_id.h>
#include <razorback/block_pool.h>
#include <razorback/event.h>
#include <razorback/judgment.h>
#include <razorback/list.h>
#include <razorback/lock.h>
#include <razorback/log.h>

#include <stdatomic.h>
#include <string.h>

struct RazorbackDevContextCapture
{
    struct RazorbackContext *context;
    List_t *judgments;
    List_t *submissions;
};

static atomic_bool sg_devModeEnabled;
static List_t *sg_devCaptures;
static Mutex_t *sg_devCaptureLock;

static int
DevCapture_Cmp(void *a, void *b)
{
    const struct RazorbackDevContextCapture *left = a;
    const struct RazorbackDevContextCapture *right = b;

    if (left == right)
        return 0;
    if (left == NULL || right == NULL)
        return -1;
    return (left->context == right->context) ? 0 : -1;
}

static int
DevCapture_KeyCmp(void *a, const void *id)
{
    const struct RazorbackDevContextCapture *capture = a;
    const struct RazorbackContext *context = id;

    if (capture == NULL || context == NULL)
        return -1;
    return (capture->context == context) ? 0 : -1;
}

static void
DevCapture_Destroy(void *item)
{
    struct RazorbackDevContextCapture *capture = item;

    if (capture == NULL)
        return;

    if (capture->judgments != NULL)
        List_Destroy(capture->judgments);
    if (capture->submissions != NULL)
        List_Destroy(capture->submissions);
    free(capture);
}

static void
DevCapture_DestroySubmission(void *item)
{
    if (item == NULL)
        return;

    (void)BlockPool_DestroyItem(item);
}

static struct Judgment *
DevMode_CloneJudgment(const struct Judgment *source)
{
    struct Judgment *copy;

    if (source == NULL)
        return NULL;

    copy = calloc(1, sizeof(*copy));
    if (copy == NULL)
        return NULL;

    uuid_copy(copy->uuidNuggetId, source->uuidNuggetId);
    copy->iSeconds = source->iSeconds;
    copy->iNanoSecs = source->iNanoSecs;
    copy->iPriority = source->iPriority;
    copy->iGID = source->iGID;
    copy->iSID = source->iSID;
    copy->Set_SfFlags = source->Set_SfFlags;
    copy->Set_EntFlags = source->Set_EntFlags;
    copy->Unset_SfFlags = source->Unset_SfFlags;
    copy->Unset_EntFlags = source->Unset_EntFlags;

    if (source->pEventId != NULL) {
        copy->pEventId = EventId_Clone(source->pEventId);
        if (copy->pEventId == NULL)
            goto error;
    }

    if (source->pBlockId != NULL) {
        copy->pBlockId = BlockId_Clone(source->pBlockId);
        if (copy->pBlockId == NULL)
            goto error;
    }

    if (source->pMetaDataList != NULL) {
        copy->pMetaDataList = List_Clone(source->pMetaDataList);
        if (copy->pMetaDataList == NULL)
            goto error;
    }

    if (source->sMessage != NULL) {
        copy->sMessage = (uint8_t *)strdup((const char *)source->sMessage);
        if (copy->sMessage == NULL)
            goto error;
    }

    return copy;

error:
    Judgment_Destroy(copy);
    return NULL;
}

static struct RazorbackDevContextCapture *
DevMode_GetCaptureLocked(struct RazorbackContext *context)
{
    if (sg_devCaptures == NULL || context == NULL)
        return NULL;

    return List_Find(sg_devCaptures, context);
}

bool
Razorback_DevMode_Initialize(void)
{
    if (sg_devCaptures != NULL && sg_devCaptureLock != NULL)
        return true;

    sg_devCaptures = List_Create(LIST_MODE_GENERIC,
                                 DevCapture_Cmp,
                                 DevCapture_KeyCmp,
                                 DevCapture_Destroy,
                                 NULL,
                                 NULL,
                                 NULL);
    sg_devCaptureLock = Mutex_Create(MUTEX_MODE_NORMAL);
    atomic_init(&sg_devModeEnabled, false);

    if (sg_devCaptures == NULL || sg_devCaptureLock == NULL) {
        if (sg_devCaptures != NULL) {
            List_Destroy(sg_devCaptures);
            sg_devCaptures = NULL;
        }
        if (sg_devCaptureLock != NULL) {
            Mutex_Destroy(sg_devCaptureLock);
            sg_devCaptureLock = NULL;
        }
        return false;
    }

    return true;
}

void
Razorback_DevMode_SetEnabled(bool enabled)
{
    atomic_store(&sg_devModeEnabled, enabled);
}

bool
Razorback_DevMode_IsEnabled(void)
{
    return atomic_load(&sg_devModeEnabled);
}

bool
Razorback_DevMode_RegisterContext(struct RazorbackContext *context)
{
    struct RazorbackDevContextCapture *capture;

    if (context == NULL)
        return false;

    if (sg_devCaptures == NULL || sg_devCaptureLock == NULL)
        return false;

    Mutex_Lock(sg_devCaptureLock);
    if (DevMode_GetCaptureLocked(context) != NULL) {
        Mutex_Unlock(sg_devCaptureLock);
        return true;
    }

    capture = calloc(1, sizeof(*capture));
    if (capture == NULL)
        goto error;

    capture->context = context;
    capture->judgments = List_Create(LIST_MODE_GENERIC,
                                     NULL,
                                     NULL,
                                     (void (*)(void *))Judgment_Destroy,
                                     NULL,
                                     NULL,
                                     NULL);
    capture->submissions = List_Create(LIST_MODE_GENERIC,
                                       NULL,
                                       NULL,
                                       DevCapture_DestroySubmission,
                                       NULL,
                                       NULL,
                                       NULL);
    if (capture->judgments == NULL || capture->submissions == NULL)
        goto error;

    if (!List_Push(sg_devCaptures, capture))
        goto error;

    Mutex_Unlock(sg_devCaptureLock);
    return true;

error:
    DevCapture_Destroy(capture);
    Mutex_Unlock(sg_devCaptureLock);
    return false;
}

void
Razorback_DevMode_UnregisterContext(struct RazorbackContext *context)
{
    if (context == NULL || sg_devCaptures == NULL || sg_devCaptureLock == NULL)
        return;

    Mutex_Lock(sg_devCaptureLock);
    (void)List_Remove(sg_devCaptures, context);
    Mutex_Unlock(sg_devCaptureLock);
}

bool
Razorback_DevMode_CaptureVerdict(struct RazorbackContext *context,
                                 const struct Judgment *judgment)
{
    struct RazorbackDevContextCapture *capture;
    struct Judgment *copy;
    bool pushed;

    if (context == NULL || judgment == NULL)
        return false;

    copy = DevMode_CloneJudgment(judgment);
    if (copy == NULL)
        return false;

    Mutex_Lock(sg_devCaptureLock);
    capture = DevMode_GetCaptureLocked(context);
    pushed = (capture != NULL) && List_Push(capture->judgments, copy);
    Mutex_Unlock(sg_devCaptureLock);

    if (!pushed)
        Judgment_Destroy(copy);

    return pushed;
}

bool
Razorback_DevMode_CaptureSubmission(struct RazorbackContext *context,
                                    struct BlockPoolItem *item)
{
    struct RazorbackDevContextCapture *capture;
    bool pushed;

    if (context == NULL || item == NULL)
        return false;

    Mutex_Lock(sg_devCaptureLock);
    capture = DevMode_GetCaptureLocked(context);
    pushed = (capture != NULL) && List_Push(capture->submissions, item);
    Mutex_Unlock(sg_devCaptureLock);
    return pushed;
}

List_t *
Razorback_DevMode_GetJudgments(struct RazorbackContext *context)
{
    struct RazorbackDevContextCapture *capture;

    if (context == NULL || sg_devCaptureLock == NULL)
        return NULL;

    Mutex_Lock(sg_devCaptureLock);
    capture = DevMode_GetCaptureLocked(context);
    Mutex_Unlock(sg_devCaptureLock);
    return (capture != NULL) ? capture->judgments : NULL;
}

List_t *
Razorback_DevMode_GetSubmissions(struct RazorbackContext *context)
{
    struct RazorbackDevContextCapture *capture;

    if (context == NULL || sg_devCaptureLock == NULL)
        return NULL;

    Mutex_Lock(sg_devCaptureLock);
    capture = DevMode_GetCaptureLocked(context);
    Mutex_Unlock(sg_devCaptureLock);
    return (capture != NULL) ? capture->submissions : NULL;
}
