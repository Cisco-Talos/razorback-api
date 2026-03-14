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
#include <razorback/block.h>
#include <razorback/block_id.h>
#include <razorback/ntlv.h>
#include <razorback/log.h>
#include <razorback/uuids.h>
#include <razorback/list.h>

#include <string.h>


SO_PUBLIC struct Block *
Block_Create (void) {
    struct Block * l_pBlock;
    if ((l_pBlock = calloc (1, sizeof (struct Block))) == NULL ) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate memory for new Block", __func__);
        return NULL;
    }
    if ((l_pBlock->pId = BlockId_Create ()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create metadata list", __func__);
        free(l_pBlock);
        return NULL;
    }

    if ((l_pBlock->pMetaDataList = NTLVList_Create ()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to create metadata list", __func__);
        free(l_pBlock);
        return NULL;
    }

    return l_pBlock;
}

SO_PUBLIC void
Block_Destroy (struct Block *p_pBlock) {
    ASSERT(p_pBlock != NULL);
    if (p_pBlock == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pBlock is NULL", __func__);
        return;
    }

    BlockId_Destroy (p_pBlock->pId);

    if (p_pBlock->pParentId != NULL) {
        BlockId_Destroy (p_pBlock->pParentId);
    }

    if (p_pBlock->pMetaDataList != NULL) {
        List_Destroy(p_pBlock->pMetaDataList);
    }

    free(p_pBlock);
}

SO_PUBLIC struct Block *
Block_Clone (const struct Block *p_pSource) {
    struct Block *l_pDestination;

    ASSERT (p_pSource != NULL);
    if (p_pSource == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pSource is NULL", __func__);
        return NULL;
    }

    if ((l_pDestination = calloc(1, sizeof (struct Block))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate new block", __func__);
        return NULL;
    }
    if ((l_pDestination->pId = BlockId_Clone (p_pSource->pId)) == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: failed to clone block ID", __func__);
        Block_Destroy (l_pDestination);
        return NULL;
    }

    if (p_pSource->pParentId == NULL) {
        l_pDestination->pParentId = NULL;
    } else {
        if ((l_pDestination->pParentId = BlockId_Clone (p_pSource->pParentId)) == NULL) {
            rzb_log (LOG_ERR, LOG_C_CORE, "%s: failed due to lack of memory", __func__);
            Block_Destroy (l_pDestination);
            return NULL;
        }
    }

    l_pDestination->pMetaDataList = List_Clone(p_pSource->pMetaDataList);
    if (l_pDestination->pMetaDataList == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE,
                "%s: failed due to failure of List_Clone", __func__);
        Block_Destroy(l_pDestination);
        return NULL;
    }

    return l_pDestination;
}

SO_PUBLIC bool
Block_MetaData_Add(struct Block *block, uuid_t uuidName, uuid_t uuidType, const uint8_t *data, uint32_t size) {
    ASSERT(block != NULL);
    ASSERT(data != NULL || size == 0);
    if (block == NULL || (data == NULL && size != 0)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: block or data is NULL", __func__);
        return false;
    }
    return NTLVList_Add(block->pMetaDataList, uuidName, uuidType, size, data);
}

SO_PUBLIC bool
Block_MetaData_Add_FileName(struct Block *block, const char * fileName) {
    ASSERT(block != NULL);
    ASSERT(fileName != NULL);
    if (block == NULL || fileName == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: block or fileName is NULL", __func__);
        return false;
    }

    uuid_t uuidName;
    uuid_t uuidType;
    if (
            UUID_Get_UUID(NTLV_NAME_FILENAME, UUID_TYPE_NTLV_NAME, uuidName) &&
            UUID_Get_UUID(NTLV_TYPE_STRING, UUID_TYPE_NTLV_TYPE, uuidType)) {
        return Block_MetaData_Add(block, uuidName, uuidType, (const uint8_t *)fileName, strlen(fileName));
    }

    rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to lookup uuids", __func__);
    return false;
}
