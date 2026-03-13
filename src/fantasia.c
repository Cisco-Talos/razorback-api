/*
 * Razorback(TM) Block Data Type Magic (fantaisa.c)
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
 *
 * For compliance with Mr Darwin's terms: this has been very significantly
 * modified from the free "file" command.
 *
 * This code has been further modified from the mod_mime_magic code by the
 * Razorback(TM) team:
 * - Compressed lookup of internal data type support has been removed
 * - File system identification code has been removed.
 * - Ascii-ness code is currently not included.
 * - All RSL code has been removed.
 * - Code integrates directly with the razorback API to set data type UUID's
 *   on items in the block pool.
 *
 * October 2011
 * Tom Judge <tjudge@sourcefire.com>
 * Sourcefire Inc,
 * 9770 Patuxent Woods Drive
 * Columbia, MD 21046,
 * United State
 */

#include "config.h"
#include "fantasia.h"
#include <razorback/uuids.h>
#include <razorback/log.h>
#include <razorback/block_pool.h>
#ifdef _MSC_VER
#include "safewindows.h"
#include <Shlwapi.h>
#include "bobins.h"
#else
#define MAGIC_FILE ETC_DIR "/magic"
#endif
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <magic.h>

#define HOWMANY  4096

static char * Magic_GetFile(void);

/*
 * apprentice - load configuration from the magic file r
 *  API request record
 */
static int
Magic_apprentice() {
    FILE *f;
    char * file = NULL;
    // TODO - Load the magic file into a buffer so we dont load it every time
    file = Magic_GetFile();
    if ((f = fopen(file, "r")) == NULL) {
        rzb_log(LOG_ERR, LOG_C_MAGIC, "%s: can't read magic file %s", __func__, file);
        return -1;
    }

    fclose(f);


    return 0;
}

bool
Magic_Init(void) {
    if (Magic_apprentice() == -1) {
        rzb_log(LOG_ERR, LOG_C_MAGIC, "%s: Magic_apprentice failed", __func__);
        return false;
    }

    return true;
}


/*
 * magic_process - process input block pool item
 * (formerly called "process" in file command, prefix added for clarity)
 * Coallesses data out of the data list into a buffer and runs magic on it.
 */
bool
Magic_process(struct BlockPoolItem *item)
{
    unsigned char buf[HOWMANY + 1];  /* one extra for terminating '\0' */
    size_t nbytes = 0;           /* number of bytes read from a datafile */
    size_t toCopy =0;
    struct BlockPoolData *dataItem = item->pDataHead;
    const char * mimetype = NULL;
    const char * err = NULL;
    bool ret = false;

    magic_t magic = magic_open( MAGIC_ERROR | MAGIC_MIME_TYPE );
    if (magic == NULL) {
        rzb_log(LOG_ERR, LOG_C_MAGIC, "%s: magic_open failed", __func__);
        return false;
    }
    // TODO - Switch to using the buffer instead of the file
    magic_load(magic, NULL);


    if (dataItem->iFlags == BLOCK_POOL_DATA_FLAG_FILE) {
        mimetype = magic_descriptor(magic, fileno(dataItem->data.file));
        rewind(dataItem->data.file);
    } else {
        memset(buf, 0, HOWMANY + 1);
        /*
         * try looking at the first HOWMANY bytes
         */
        while (nbytes < HOWMANY && dataItem != NULL) {
            toCopy = ((nbytes + dataItem->iLength) > HOWMANY ? (HOWMANY - nbytes) : dataItem->iLength);
            rzb_log(LOG_DEBUG, LOG_C_MAGIC, "%s: Coallessing data buffers Have=%zu Getting=%zu Result=%zu", __func__, nbytes, toCopy, nbytes+toCopy);
            memcpy(buf+nbytes, dataItem->data.pointer, toCopy);
            nbytes += toCopy;
            dataItem = dataItem->pNext;
        }

        buf[nbytes++] = '\0';  /* null-terminate it */
        mimetype = magic_buffer(magic, buf, nbytes);
    }
    rzb_log(LOG_DEBUG, LOG_C_MAGIC, "%s: mimetype=%s", __func__, mimetype);
    if (mimetype == NULL) {
        err = magic_error(magic);
        if (err != NULL) {
            rzb_log(LOG_ERR, LOG_C_MAGIC, "%s: magic_(descriptor|buffer) failed: %s", __func__, err);
            ret = false;
        } else {
            ret = true;
        }
    } else {
        ret = UUID_Get_UUID(mimetype, UUID_TYPE_DATA_TYPE, item->pEvent->pBlock->pId->uuidDataType);
    }
    magic_close(magic);
    return ret;
}


#ifdef _MSC_VER
// Worlds largest kludge
#undef LONG
static char *
Magic_GetFile(void)
{
    char *path;
    HKEY hkey = HKEY_LOCAL_MACHINE;
    HKEY razorback;
    LONG lRet;
    DWORD type, RegValueLen;

    lRet = RegOpenKeyA(
        hkey,
        "SOFTWARE\\Razorback",
        &razorback);

    if(lRet != ERROR_SUCCESS) {
        rzb_log(LOG_ERR, "%s: Failed because registry key does not exist. SOFTWARE", __func__);
        return false;
    }
    if ((path = calloc(MAX_PATH, sizeof(char)))== NULL)
        return NULL;

    lRet = RegQueryValueExA(
        razorback,
        "Path",
        NULL,
        &type,
        NULL,
        &RegValueLen);

    if (lRet != ERROR_SUCCESS) {
        rzb_log(LOG_ERR, "%s: Failed to query registry value length", __func__);
        return false;
    }
    if (RegValueLen > MAX_PATH -7)
    {
        rzb_log(LOG_ERR, "%s: Key to large", __func__);
        return false;
    }

    lRet = RegQueryValueExA(
        razorback,
        "Path",
        NULL,
        &type,
        (LPBYTE)path,
        &RegValueLen);

    if (lRet != ERROR_SUCCESS) {
        rzb_log(LOG_ERR, "%s: Failed to query registry value", __func__);
        return false;
    }

    if (type != REG_SZ) {
        rzb_log(LOG_ERR, "%s: Failed because registry key is not the right type", __func__);
        return false;
    }

    PathAppendA(path, "magic");
    return path;
}
#else //_MSC_VER
static char * Magic_GetFile(void)
{
    return (char *)MAGIC_FILE;
}
#endif
