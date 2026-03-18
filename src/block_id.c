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
#include <razorback/block_id.h>
#include <razorback/hash.h>
#include <razorback/log.h>

#include <string.h>
#include <stdio.h>

#include "runtime_config.h"

SO_PUBLIC bool
BlockId_IsEqual (const struct BlockId * p_pA, const struct BlockId * p_pB) {
    bool uuid, hash, length;
    ASSERT (p_pA != NULL);
    ASSERT (p_pB != NULL);
    if (p_pA == NULL || p_pB == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pA or p_pB is NULL", __func__);
        return false;
    }

    // check if pointers equal
    if (p_pA == p_pB) {
        return true;
    }

    uuid = (uuid_compare (p_pA->uuidDataType, p_pB->uuidDataType) == 0);
    hash = Hash_IsEqual (p_pA->pHash, p_pB->pHash);
    length = (p_pA->iLength == p_pB->iLength);
    return (uuid && hash && length);
}

SO_PUBLIC char *
BlockId_ToText (const struct BlockId *p_pA) {
    char l_sUUID[UUID_STRING_LENGTH];
    char *l_sHash;
    char *text;

    ASSERT (p_pA != NULL);
    if (p_pA == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pA is NULL", __func__);
        return NULL;
    }

    uuid_unparse (p_pA->uuidDataType, l_sUUID);
    l_sHash = Hash_ToText (p_pA->pHash);
    if (l_sHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to convert hash to text", __func__);
        return NULL;
    }
    if (asprintf(&text, "%s-%8.8jx-%s", l_sUUID, p_pA->iLength, l_sHash) == -1) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to allocate block id text", __func__);
        free(l_sHash);
        return NULL;
    }
    free(l_sHash);
    return text;
}

struct BlockId *
BlockId_Create_Shallow (void) {
    return calloc(1, sizeof(struct BlockId));
}

SO_PUBLIC struct BlockId *
BlockId_Create (void) {
    struct BlockId *id;

    if ((id = BlockId_Create_Shallow()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate block id", __func__);
        return NULL;
    }
    // intialize the hash
    if ((id->pHash = Hash_Create()) == NULL) {
        BlockId_Destroy(id);
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed due to lack of memory: Hash_Create", __func__);
        return NULL;
    }
    return id;
}

SO_PUBLIC uint32_t
BlockId_StringLength (struct BlockId *p_pB)
{
    ASSERT (p_pB != NULL);
    if (p_pB == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pB is NULL", __func__);
        return 0;
    }
    // return the value
    return UUID_STRING_LENGTH + Hash_StringLength (p_pB->pHash) + 9;  // "%s-%8.8x-%s"
}

SO_PUBLIC void
BlockId_Destroy (struct BlockId *p_pBlockId) {
    ASSERT (p_pBlockId != NULL);
    if (p_pBlockId == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pBlockId is NULL", __func__);
        return;
    }
    if (p_pBlockId->pHash != NULL) {
        Hash_Destroy(p_pBlockId->pHash);
    }
    free(p_pBlockId);
}

SO_PUBLIC struct BlockId *
BlockId_Clone (const struct BlockId *p_pSource) {
    struct BlockId *dest;
    ASSERT (p_pSource != NULL);
    if (p_pSource == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pSource is NULL", __func__);
        return NULL;
    }

    if ((dest = BlockId_Create_Shallow()) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: failed due to lack of memory", __func__);
        return NULL;
    }

    if ((dest->pHash = Hash_Clone (p_pSource->pHash)) == NULL) {
        BlockId_Destroy(dest);
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: failed due to failure of Hash_Clone", __func__);
        return NULL;
    }

    uuid_copy (dest->uuidDataType, p_pSource->uuidDataType);
    dest->iLength = p_pSource->iLength;

    // done
    return dest;
}
