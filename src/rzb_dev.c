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

#include "api_internal.h"
#include "dev_mode.h"
#include "nugget_tool.h"
#include "transfer/core.h"

#include <razorback/api.h>
#include <razorback/block.h>
#include <razorback/block_pool.h>
#include <razorback/judgment.h>
#include <razorback/list.h>
#include <razorback/log.h>
#include <razorback/metadata.h>
#include <razorback/ntlv.h>
#include <razorback/thread.h>
#include <razorback/transfer.h>
#include <razorback/uuids.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

struct DevContextLookup
{
    struct RazorbackContext *context;
    size_t count;
};

struct DevInspectRun
{
    struct RazorbackContext *context;
    struct BlockPoolItem *inputItem;
    List_t *eventMetadata;
    uint8_t result;
};

static void
RzbDev_Usage(const char *name)
{
    fprintf(stderr, "Usage: %s [--debug] [--type=DATA_TYPE] <nugget-module> <file>\n",
            name);
}

static const char *
RzbDev_ResultLabel(uint8_t result)
{
    switch (result) {
    case JUDGMENT_REASON_DONE:
        return "done";
    case JUDGMENT_REASON_ALERT:
        return "alert";
    case JUDGMENT_REASON_DEFERRED:
        return "deferred";
    case JUDGMENT_REASON_ERROR:
    default:
        return "error";
    }
}

static const char *
RzbDev_BlockStatusLabel(uint32_t status)
{
    switch (status & BLOCK_POOL_STATUS_MASK) {
    case BLOCK_POOL_STATUS_COLLECTING:
        return "collecting";
    case BLOCK_POOL_STATUS_FINALIZED:
        return "finalized";
    case BLOCK_POOL_STATUS_CHECK_GLOBAL_CACHE:
        return "check_global_cache";
    case BLOCK_POOL_STATUS_CHECKING_GLOBAL_CACHE:
        return "checking_global_cache";
    case BLOCK_POOL_STATUS_SUBMIT_DATA:
        return "submit_data";
    case BLOCK_POOL_STATUS_PAGED:
        return "paged";
    case BLOCK_POOL_STATUS_DESTROY:
        return "destroy";
    case BLOCK_POOL_STATUS_ERROR:
        return "error";
    case BLOCK_POOL_STATUS_NO_TYPE:
        return "no_type";
    default:
        return "unknown";
    }
}

static int
RzbDev_SelectContext(struct RazorbackContext *context, void *userData)
{
    struct DevContextLookup *lookup = userData;

    if (lookup == NULL || context == NULL)
        return LIST_EACH_ERROR;

    lookup->context = context;
    lookup->count++;
    return LIST_EACH_OK;
}

static struct RazorbackContext *
RzbDev_GetOnlyContext(void)
{
    struct DevContextLookup lookup;

    lookup.context = NULL;
    lookup.count = 0;
    if (!Razorback_ForEach_Context(RzbDev_SelectContext, &lookup))
        return NULL;
    if (lookup.count != 1)
        return NULL;
    return lookup.context;
}

static char *
RzbDev_HashToString(const struct Hash *hash)
{
    char *value;
    uint32_t i;

    if (hash == NULL || hash->pData == NULL || hash->iSize == 0)
        return NULL;

    value = calloc((size_t)hash->iSize * 2U + 1U, sizeof(char));
    if (value == NULL)
        return NULL;

    for (i = 0; i < hash->iSize; ++i)
        snprintf(value + (i * 2U), 3U, "%02x", hash->pData[i]);

    return value;
}

static int
RzbDev_PrintJudgment(void *item, void *userData)
{
    struct Judgment *judgment = item;
    size_t *index = userData;
    char *message;
    char *hashValue;

    if (judgment == NULL || index == NULL)
        return LIST_EACH_ERROR;

    (*index)++;
    hashValue = (judgment->pBlockId != NULL) ? RzbDev_HashToString(judgment->pBlockId->pHash) : NULL;
    message = (judgment->sMessage != NULL) ? strdup((const char *)judgment->sMessage) : NULL;

    printf("judgment[%zu]: gid=%u sid=%u priority=%u sf_set=0x%08x ent_set=0x%08x\n",
           *index, judgment->iGID, judgment->iSID, judgment->iPriority,
           judgment->Set_SfFlags, judgment->Set_EntFlags);
    if (message != NULL)
        printf("  message: %s\n", message);
    if (hashValue != NULL)
        printf("  block_hash: %s\n", hashValue);

    free(message);
    free(hashValue);
    return LIST_EACH_OK;
}

static int
RzbDev_PrintSubmission(void *item, void *userData)
{
    struct BlockPoolItem *poolItem = item;
    size_t *index = userData;
    char *typeName;
    char *hashValue;

    if (poolItem == NULL || index == NULL)
        return LIST_EACH_ERROR;

    (*index)++;
    typeName = UUID_Get_NameByUUID(poolItem->pEvent->pBlock->pId->uuidDataType,
                                   UUID_TYPE_DATA_TYPE);
    hashValue = RzbDev_HashToString(poolItem->pEvent->pBlock->pId->pHash);

    printf("submission[%zu]: status=%s type=%s length=%llu\n",
           *index,
           RzbDev_BlockStatusLabel(poolItem->iStatus),
           (typeName != NULL) ? typeName : "unknown",
           (unsigned long long)poolItem->pEvent->pBlock->pId->iLength);
    if (hashValue != NULL)
        printf("  hash: %s\n", hashValue);

    free(typeName);
    free(hashValue);
    return LIST_EACH_OK;
}

static void
RzbDev_InspectThread(Thread_t *thread)
{
    struct DevInspectRun *run;
    struct RazorbackContext *context;
    struct Block *block = NULL;
    char *fileName = NULL;
    void *threadData = NULL;

    run = Thread_GetUserData(thread);
    context = Thread_GetContext(thread);
    run->result = JUDGMENT_REASON_ERROR;

    if (run == NULL || context == NULL || run->inputItem == NULL)
        return;

    if (context->inspector.hooks->initThread != NULL &&
        !context->inspector.hooks->initThread(&threadData))
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Nugget initThread failed", __func__);
        goto cleanup;
    }

    block = Block_Clone(run->inputItem->pEvent->pBlock);
    if (block == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to clone input block", __func__);
        goto cleanup;
    }

    if (run->inputItem->pDataHead == NULL ||
        run->inputItem->pDataHead->data.fileName == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Local input block is missing file data", __func__);
        goto cleanup;
    }

    fileName = strdup(run->inputItem->pDataHead->data.fileName);
    if (fileName == NULL)
        goto cleanup;

    if (!Transfer_Prepare_File(block, fileName, false)) {
        fileName = NULL;
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to prepare file-backed block", __func__);
        goto cleanup;
    }
    fileName = NULL;

    run->result = context->inspector.hooks->processBlock(block,
                                                         run->inputItem->pEvent->pId,
                                                         run->eventMetadata,
                                                         threadData);

cleanup:
    free(fileName);
    if (block != NULL) {
        Transfer_Free(block, NULL);
        Block_Destroy(block);
    }
    if (context != NULL && context->inspector.hooks->cleanupThread != NULL)
        context->inspector.hooks->cleanupThread(threadData);
}

int
main(int argc, char **argv)
{
    static const struct option long_options[] = {
        {"debug", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {"type", required_argument, NULL, 't'},
        {NULL, 0, NULL, 0}
    };
    struct NuggetToolModule module;
    struct RazorbackContext *context;
    struct BlockPoolItem *inputItem = NULL;
    Thread_t *inspectThread = NULL;
    struct DevInspectRun run;
    List_t *judgments;
    List_t *submissions;
    const char *typeNameArg = NULL;
    const char *modulePath;
    const char *filePath;
    char *autoTypeName = NULL;
    size_t index;
    int opt;
    int exitCode = 1;
    bool debug = false;
    bool moduleInitialized = false;

    memset(&run, 0, sizeof(run));

    while ((opt = getopt_long(argc, argv, "dht:", long_options, NULL)) != -1) {
        switch (opt) {
        case 'd':
            debug = true;
            break;
        case 'h':
            RzbDev_Usage(argv[0]);
            return 0;
        case 't':
            typeNameArg = optarg;
            break;
        default:
            RzbDev_Usage(argv[0]);
            return 1;
        }
    }

    if ((argc - optind) != 2) {
        RzbDev_Usage(argv[0]);
        return 1;
    }

    modulePath = argv[optind];
    filePath = argv[optind + 1];

    RZB_Init_API();
    if (debug)
        rzb_debug_logging();
    Razorback_DevMode_SetEnabled(true);

    if (!NuggetTool_LoadModule(modulePath, &module))
        return 1;

    if (!module.initNug()) {
        rzb_log(LOG_ERR, LOG_C_NUGGET, "%s: Nugget initialization failed for %s",
                __func__, modulePath);
        goto cleanup;
    }
    moduleInitialized = true;

    context = RzbDev_GetOnlyContext();
    if (context == NULL || context->inspector.hooks == NULL ||
        context->inspector.hooks->processBlock == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: Expected exactly one inspection context after initNug()",
                __func__);
        goto cleanup;
    }

    inputItem = BlockPool_CreateItem(context);
    if (inputItem == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create local input item", __func__);
        goto cleanup;
    }

    if (typeNameArg != NULL) {
        if (!BlockPool_SetItemDataType(inputItem, (char *)typeNameArg)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to set input data type '%s'",
                    __func__, typeNameArg);
            goto cleanup;
        }
    } else if (context->inspector.dataTypeCount == 1) {
        autoTypeName = UUID_Get_NameByUUID(context->inspector.dataTypeList[0],
                                           UUID_TYPE_DATA_TYPE);
        if (autoTypeName != NULL &&
            !BlockPool_SetItemDataType(inputItem, autoTypeName))
        {
            rzb_log(LOG_ERR, LOG_C_CORE,
                    "%s: Failed to set inferred input data type '%s'",
                    __func__, autoTypeName);
            goto cleanup;
        }
    }

    if (!BlockPool_AddData_FromFile(inputItem, (char *)filePath, false)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to attach input file '%s'",
                __func__, filePath);
        goto cleanup;
    }

    (void)Block_MetaData_Add_FileName(inputItem->pEvent->pBlock, filePath);

    if (!BlockPool_FinalizeItem(inputItem)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to finalize local input item", __func__);
        goto cleanup;
    }

    if (uuid_is_null(inputItem->pEvent->pBlock->pId->uuidDataType) == 1) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: Input file type was not resolved; pass --type for this nugget",
                __func__);
        goto cleanup;
    }

    run.context = context;
    run.inputItem = inputItem;
    run.eventMetadata = NTLVList_Create();
    run.result = JUDGMENT_REASON_ERROR;
    if (run.eventMetadata == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate event metadata list", __func__);
        goto cleanup;
    }
    (void)Metadata_Add_Filename(run.eventMetadata, filePath);

    inspectThread = Thread_Launch(RzbDev_InspectThread, &run, "Local Inspect", context);
    if (inspectThread == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to launch local inspection thread",
                __func__);
        goto cleanup;
    }

    Thread_Join(inspectThread);
    Thread_Destroy(inspectThread);
    inspectThread = NULL;

    printf("inspection_result: %s\n", RzbDev_ResultLabel(run.result));

    judgments = Razorback_DevMode_GetJudgments(context);
    submissions = Razorback_DevMode_GetSubmissions(context);

    printf("judgment_count: %zu\n", (judgments != NULL) ? List_Length(judgments) : 0U);
    index = 0;
    if (judgments != NULL)
        (void)List_ForEach(judgments, RzbDev_PrintJudgment, &index);

    printf("submission_count: %zu\n", (submissions != NULL) ? List_Length(submissions) : 0U);
    index = 0;
    if (submissions != NULL)
        (void)List_ForEach(submissions, RzbDev_PrintSubmission, &index);

    exitCode = 0;

cleanup:
    if (inspectThread != NULL) {
        Thread_Join(inspectThread);
        Thread_Destroy(inspectThread);
    }
    if (inputItem != NULL)
        BlockPool_DestroyItem(inputItem);
    if (run.eventMetadata != NULL)
        List_Destroy(run.eventMetadata);
    free(autoTypeName);
    if (moduleInitialized && module.shutdownNug != NULL)
        module.shutdownNug();
    NuggetTool_UnloadModule(&module);
    Razorback_DevMode_SetEnabled(false);
    return exitCode;
}
