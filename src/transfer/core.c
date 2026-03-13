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
#include <razorback/api.h>
#include <razorback/debug.h>
#include <razorback/log.h>
#include <razorback/hash.h>
#include <razorback/thread.h>
#include <razorback/list.h>
#include "transfer/core.h"
#include "runtime_config.h"
#ifdef _MSC_VER
#include "bobins.h"
#else //_MSC_VER
#include <sys/mman.h>
#include <errno.h>
#include <dlfcn.h>

#endif //_MSC_VER

#define RETRIES 5

List_t *sg_transportList = NULL;
static int Transport_Cmp(void *a, void *b);
static int Transport_KeyCmp(void *a, void *key);
static bool sg_bTraditionalMode = true;

char *
Transfer_generateFilename (struct Block *block)
{
    char *hash;
    char *filename;
    if ((hash = Hash_ToText (block->pId->pHash)) == NULL)
    {
        rzb_log (LOG_ERR,LOG_C_TRANSFER, "%s: Could not convert hash to text", __func__);
        return NULL;
    }
#ifdef _MSC_VER
#define FILENAME_FMT "%s.%u"
#else
#define FILENAME_FMT "%s.%ju"
#endif
    if (asprintf(&filename, FILENAME_FMT, hash, block->pId->iLength) == -1)
    {
        free(hash);
        return NULL;
    }
    free(hash);
    return filename;
}

void * pluginDlHandle;
bool (*initPlugin)(void);

bool
Transfer_Init(void)
{
    char * mode = Config_getTransferMode();
    if ((mode == NULL) || (strcmp(mode, "") ==0) ){
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Global.TransferMode not set in API config file.", __func__);
        return false;
    }
    sg_transportList = List_Create(LIST_MODE_GENERIC,
                                   Transport_Cmp,
                                   Transport_KeyCmp,
                                   NULL,
                                   NULL, // Clone
                                   NULL, // Lock
                                   NULL); // Unlock
    if (sg_transportList == NULL)
        return false;

    if (!File_Init())
        return false;
    if (!SSH_Init())
        return false;
    if (!HTTP_Init())
        return false;

    if (strcmp(mode, "traditional") == 0) {
        rzb_log(LOG_INFO,LOG_C_TRANSFER, "%s: File transfers taking place in traditional mode", __func__);
        sg_bTraditionalMode = true;
    } else {
        rzb_log(LOG_INFO,LOG_C_TRANSFER, "%s: File transfers taking place using plugin: %s", __func__, mode);
        sg_bTraditionalMode = false;
        char * soVersion;
        int l_iIt, l_iLen;
        if ((l_iLen = asprintf(&soVersion, "%s", NUGGET_SO_VERSION))== -1)
            return false;

        // This is ugly there must be a better way
        for (l_iIt =0; l_iIt < l_iLen; l_iIt++)
            if (soVersion[l_iIt] == ':')
                soVersion[l_iIt] = '.';

        char * soFile;
        if (asprintf(&soFile, "%s/razorback_transfer_%s.so.%s", LIB_DIR, mode, soVersion) == -1) {
            rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: Failed to allocate plugin path", __func__);
            free(soVersion);
            return false;
        }
        free(soVersion);
        soVersion = NULL;

        if ((pluginDlHandle = dlopen(soFile, RTLD_LOCAL | RTLD_NOW)) == NULL) {
            char * errstr = dlerror();

            rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to open %s, %s", __func__, soFile, errstr);
            free(soFile);
            return false;
        }
        free(soFile);
        soFile = NULL;

        *(void **)&initPlugin = dlsym(pluginDlHandle, "transferInit");
       if (initPlugin == NULL) {
            rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed find plugin init function", __func__);
            return false;
        }

        if (!initPlugin()) {
            rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to initialize transfer plugin", __func__);
            return false;
        }
    }
    return true;
}

bool
Transport_Register(struct TransportDescriptor *desc)
{
    return List_Push(sg_transportList, desc);
}

bool
Transport_IsSupported(uint8_t protocol)
{
    struct TransportDescriptor *trans= NULL;
    trans = List_Find(sg_transportList, &protocol);
    return !(trans == NULL);
}

enum TransferStatus
Transfer_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher)
{
    struct TransportDescriptor *trans = NULL;
    int i;
    enum TransferStatus status;

    if ((trans = List_Find(sg_transportList, &dispatcher->dispatcher->protocol)) == NULL) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "Failed to find transport descriptor for protocol: %u", dispatcher->dispatcher->protocol);
        return TRANSFER_FAIL_LOCAL;
    }
    rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: locality: %u, protocol: %u", __func__, dispatcher->locality, dispatcher->dispatcher->protocol);
    rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Transport: %s", __func__, trans->name);

    for (i =0; i < RETRIES; i++)
    {
        status = trans->store(item, dispatcher);
        if (status == TRANSFER_OK)
            break;
    }
    return status;
}

enum TransferStatus
Transfer_Fetch(struct Block *block, struct ConnectedEntity *dispatcher)
{
    struct TransportDescriptor *trans = NULL;
    int i;
    enum TransferStatus status;

    if ((trans = List_Find(sg_transportList, &dispatcher->dispatcher->protocol)) == NULL) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to find transport descriptor", __func__);
        return TRANSFER_FAIL_LOCAL;
    }
    rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: locality: %u, protocol: %u", __func__, dispatcher->locality, dispatcher->dispatcher->protocol);
    rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Transport: %s", __func__, trans->name);


    for (i = 0; i < RETRIES; i++)
    {
        status = trans->fetch(block, dispatcher);
        if (status == TRANSFER_OK)
            break;
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Retrying transfer", __func__);
    }
    return status;
}

SO_PUBLIC bool
Transfer_Prepare_File(struct Block *block, char *file, bool temp)
{
    ASSERT(file != NULL);
    if (file == NULL)
    {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: File is null", __func__);
        return false;
    }
    if ((block->data.file=fopen(file, "r")) == NULL)
    {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to open file handle: %s, File: %s", strerror(errno), file);
        return false;
    }
    block->data.fileName = file;
    block->data.tempFile = temp;


#ifdef _MSC_VER
again:
    block->data.mfileHandle = CreateFileA(block->data.fileName, GENERIC_READ, FILE_SHARE_READ,  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (block->data.mfileHandle == NULL || block->data.mfileHandle == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_SHARING_VIOLATION)
        {
            //rzb_log(LOG_ERR, "Sharing volation retry");
            goto again;
        }

        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to create file handle: File: %s Error: %d", __func__, block->data.fileName, GetLastError());
        return false;
    }
    block->data.mapHandle = CreateFileMapping(block->data.mfileHandle, NULL, PAGE_READONLY, 0,0, NULL);
    if (block->data.mapHandle == NULL)
    {
        CloseHandle(block->data.mfileHandle);
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to create file mapping: %d", __func__, GetLastError());
        return false;
    }
    block->data.pointer = MapViewOfFile(block->data.mapHandle, FILE_MAP_READ, 0,0,0);
    if (block->data.pointer == NULL)
    {
        CloseHandle(block->data.mfileHandle);
        CloseHandle(block->data.mapHandle);
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to create map view", __func__);
        return false;
    }
#else //_MSC_VER
    block->data.pointer = mmap (NULL, block->pId->iLength, PROT_READ, MAP_PRIVATE, fileno(block->data.file), 0);
    if (block->data.pointer == MAP_FAILED)
    {
        rzb_perror(LOG_C_TRANSFER,"%s");
        block->data.pointer = NULL;
        fclose(block->data.file);
        return false;
    }
#endif //_MSC_VER
    return true;
}

void
Transfer_Free(struct Block *block, struct ConnectedEntity *dispatcher)
{
    //rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Called for %s", __func__, block->data.fileName);
    if (block->data.pointer != NULL)
    {
#ifdef _MSC_VER
        UnmapViewOfFile(block->data.pointer);
        if (CloseHandle(block->data.mapHandle) == 0)
            rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Thread ID: %d Failed to close map handle: %d, File: %s", __func__, Thread_GetCurrent()->iThread, GetLastError(), block->data.fileName);
        if (CloseHandle(block->data.mfileHandle) == 0)
            rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Thread ID: %d Failed to close file handle: %d, File: %s", __func__, Thread_GetCurrent()->iThread, GetLastError(), block->data.fileName);

#else //_MSC_VER
        munmap(block->data.pointer, block->pId->iLength);
#endif
    }

    if (block->data.file != NULL)
        fclose(block->data.file);

    // If it was a temp file delete it
    if (block->data.tempFile)
        unlink(block->data.fileName);

    if (block->data.fileName != NULL)
        free(block->data.fileName);
}

static int
Transport_Cmp(void *a, void *b)
{
    struct TransportDescriptor *dA = a;
    struct TransportDescriptor *dB = b;
    if (dA == dB)
        return 0;
    return (dA->id - dB->id);
}
static int
Transport_KeyCmp(void *a, void *key)
{
    struct TransportDescriptor *dA = a;
    uint8_t *id = key;
    return (dA->id - *id);
}
