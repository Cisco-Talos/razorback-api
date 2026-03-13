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
#include <razorback/hash.h>
#include <razorback/log.h>
#include <stdio.h>
#include <string.h>

#include <openssl/evp.h>

#include "runtime_config.h"

SO_PUBLIC bool
Hash_IsEqual (const struct Hash *p_pHashA, const struct Hash *p_pHashB) {
    ASSERT (p_pHashA != NULL);
    if (p_pHashA == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHashA is NULL", __func__);
        return false;
    }
    ASSERT (p_pHashB != NULL);
    if (p_pHashB == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHashB is NULL", __func__);
        return false;
    }
    ASSERT (p_pHashA->pData != NULL);
    if (p_pHashA->pData == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHashA->pData is NULL", __func__);
        return false;
    }
    ASSERT (p_pHashB->pData != NULL);
    if (p_pHashB->pData == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHashB->pData is NULL", __func__);
        return false;
    }

    ASSERT (p_pHashA->iFlags & HASH_FLAG_FINAL);
    if (!(p_pHashA->iFlags & HASH_FLAG_FINAL)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHashA is not finalized", __func__);
        return false;
    }

    ASSERT (p_pHashB->iFlags & HASH_FLAG_FINAL);
    if (!(p_pHashB->iFlags & HASH_FLAG_FINAL)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHashB is not finalized", __func__);
        return false;
    }

    if (p_pHashA == p_pHashB) {
        return true;
    }
    if (p_pHashA->iSize != p_pHashB->iSize) {
        return false;
    }
    return (memcmp (p_pHashA->pData, p_pHashB->pData, p_pHashA->iSize) == 0);
}

SO_PUBLIC char *
Hash_ToText (const struct Hash *p_pHash)
{
    char *l_sString = NULL;
    uint32_t l_iIndex;
    char *l_sTemp;

    ASSERT (p_pHash != NULL);
    if (p_pHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is NULL", __func__);
        return NULL;
    }
    ASSERT (p_pHash->iFlags & HASH_FLAG_FINAL);
    if (!(p_pHash->iFlags & HASH_FLAG_FINAL)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is not finalized", __func__);
        return NULL;
    }

    if ((l_sString = (char *)calloc((p_pHash->iSize * 2) + 1, sizeof(char))) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate new string", __func__);
        return NULL;
    }

    l_sTemp = l_sString;

    for(l_iIndex=0; l_iIndex< p_pHash->iSize; l_iIndex++){
        snprintf(l_sTemp, 3, "%02x", p_pHash->pData[l_iIndex]);
        l_sTemp+=(char)2;
    }
    return l_sString;
}

SO_PUBLIC struct Hash *
Hash_Create (void) {
    return Hash_Create_Type(Config_getHashType());
}

static const char *
Hash_Get_OpenSSL_Name(uint32_t type)
{
    switch (type) {
        case HASH_TYPE_MD5:
            return "MD5";
        case HASH_TYPE_SHA1:
            return "SHA1";
        case HASH_TYPE_SHA224:
            return "SHA224";
        case HASH_TYPE_SHA256:
            return "SHA256";
        case HASH_TYPE_SHA512:
            return "SHA512";
        default:
            return NULL;
    }
}

static bool
Hash_Init_OpenSSL(struct Hash *hash) {
    const char *algorithmName;
    ASSERT(hash != NULL);
    if (hash == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: failed due to NULL hash", __func__);
        return false;
    }
    hash->pData = (uint8_t *)calloc (EVP_MAX_MD_SIZE, sizeof (uint8_t));
    if (hash->pData == NULL) {
        rzb_log (LOG_ERR, LOG_C_CORE, "%s: failed due to lack of memory", __func__);
        return false;
    }

    algorithmName = Hash_Get_OpenSSL_Name(hash->iType);
    if (algorithmName == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed due to invalid type", __func__);
        return false;
    }

    if ((hash->md = EVP_MD_fetch(NULL, algorithmName, NULL)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to fetch digest %s", __func__, algorithmName);
        return false;
    }

    if ((hash->CTX = EVP_MD_CTX_new()) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate digest context", __func__);
        return false;
    }

    if (EVP_DigestInit_ex2(hash->CTX, hash->md, NULL) != 1) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize digest context", __func__);
        return false;
    }

    return true;
}

SO_PUBLIC struct Hash *
Hash_Create_Type (uint32_t p_iType) {
    struct Hash *l_pHash;
    if ((l_pHash = (struct Hash *) calloc(1, sizeof(struct Hash))) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate new hash", __func__);
        return NULL;
    }

    l_pHash->iFlags=0;
    l_pHash->iType = p_iType;

    if (!Hash_Init_OpenSSL(l_pHash)) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize OpenSSL", __func__);
        Hash_Destroy(l_pHash);
        return NULL;
    }
    return l_pHash;
}

SO_PUBLIC struct Hash *
Hash_Create_From_String(uint32_t p_iType, const char *p_sHash) {
    const char *pos;
    size_t i;
    ASSERT(p_sHash != NULL);
    if (p_sHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_sHash is NULL", __func__);
        return NULL;
    }

    struct Hash *hash = NULL;

    if ((hash = calloc(1, sizeof (struct Hash))) == NULL) {
        return false;
    }

// TODO: Validate string based on type
    hash->iType = p_iType;

    hash->iSize = strlen(p_sHash)/2;
    if ((hash->pData = calloc(hash->iSize, sizeof(uint8_t))) == NULL) {
        Hash_Destroy(hash);
        return NULL;
    }
    pos = p_sHash;
    for(i = 0; i < hash->iSize; i++) {
#ifdef _MSC_VER
        tmp[0] = *pos;
        tmp[1] = *(pos+1);
        b = strtoul(tmp,NULL, 16);
        hash->pData[i] = (uint8_t) b;
#else
        sscanf(pos, "%2hhx", &hash->pData[i]);
#endif
        pos += 2;
    }
    hash->iFlags = HASH_FLAG_FINAL;
    return hash;
}

SO_PUBLIC bool
Hash_Update (struct Hash * p_pHash, uint8_t * p_pData, uint32_t p_iLength)
{
    ASSERT (p_pHash != NULL);
    if (p_pHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is NULL", __func__);
        return false;
    }
    ASSERT (p_pHash->pData != NULL);
    if (p_pHash->pData == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash->pData is NULL", __func__);
        return false;
    }
    ASSERT (p_pHash->iType > 0);
    if (p_pHash->iType <= 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash->iType is invalid", __func__);
        return false;
    }
    ASSERT (!(p_pHash->iFlags & HASH_FLAG_FINAL));
    if (p_pHash->iFlags & HASH_FLAG_FINAL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is already finalized", __func__);
        return false;
    }
    if (EVP_DigestUpdate(p_pHash->CTX, p_pData, p_iLength) != 1) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to update digest", __func__);
        return false;
    }
    return true;
}

SO_PUBLIC bool
Hash_Update_File (struct Hash * p_pHash, FILE *file)
{
    uint8_t data[4096];
    size_t len;
    ASSERT (p_pHash != NULL);
    if (p_pHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is NULL", __func__);
        return false;
    }
    ASSERT (p_pHash->pData != NULL);
    if (p_pHash->pData == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash->pData is NULL", __func__);
        return false;
    }
    ASSERT (p_pHash->iType > 0);
    if (p_pHash->iType <= 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash->iType is invalid", __func__);
        return false;
    }
    ASSERT (!(p_pHash->iFlags & HASH_FLAG_FINAL));
    if (p_pHash->iFlags & HASH_FLAG_FINAL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is already finalized", __func__);
        return false;
    }
    ASSERT(file != NULL);
    if (file == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: file is NULL", __func__);
        return false;
    }

    while((len = fread(data,1,4096, file)) > 0)
    {
        if (EVP_DigestUpdate(p_pHash->CTX, data, len) != 1) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to update digest from file", __func__);
            rewind(file);
            return false;
        }
    }
    rewind(file);
    return true;
}

SO_PUBLIC bool
Hash_Finalize (struct Hash * p_pHash) {
    unsigned int digestSize;

    ASSERT (p_pHash != NULL);
    if (p_pHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is NULL", __func__);
        return false;
    }
    ASSERT (p_pHash->pData != NULL);
    if (p_pHash->pData == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash->pData is NULL", __func__);
        return false;
    }
    ASSERT (p_pHash->iType > 0);
    if (p_pHash->iType <= 0) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash->iType is invalid", __func__);
        return false;
    }
    ASSERT (!(p_pHash->iFlags & HASH_FLAG_FINAL));
    if (p_pHash->iFlags & HASH_FLAG_FINAL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is already finalized", __func__);
        return false;
    }

    if (EVP_DigestFinal_ex(p_pHash->CTX, p_pHash->pData, &digestSize) != 1) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to finalize digest", __func__);
        return false;
    }

    p_pHash->iSize = digestSize;
    p_pHash->iFlags = p_pHash->iFlags | HASH_FLAG_FINAL;
    return true;
}

SO_PUBLIC uint32_t
Hash_BinaryLength (struct Hash *p_pHash) {
    ASSERT(p_pHash != NULL);
    if (p_pHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is NULL", __func__);
        return 0;
    }
    return p_pHash->iSize + (sizeof (uint32_t)*2);
}

SO_PUBLIC uint32_t
Hash_DigestLength (struct Hash *p_pHash) {
    ASSERT(p_pHash != NULL);
    if (p_pHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is NULL", __func__);
        return 0;
    }
    return p_pHash->iSize;
}

SO_PUBLIC uint32_t
Hash_StringLength (struct Hash *p_pHash) {
    ASSERT(p_pHash != NULL);
    if (p_pHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is NULL", __func__);
        return 0;
    }
    return (p_pHash->iSize * 2) + 1;
}

SO_PUBLIC void
Hash_Destroy (struct Hash *p_pHash) {
    ASSERT (p_pHash != NULL);
    if (p_pHash == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pHash is NULL", __func__);
        return;
    }

    if (p_pHash->pData != NULL) {
        free(p_pHash->pData);
    }

    if (p_pHash->CTX != NULL) {
        EVP_MD_CTX_free(p_pHash->CTX);
    }
    if (p_pHash->md != NULL) {
        EVP_MD_free(p_pHash->md);
    }
    free(p_pHash);
}

SO_PUBLIC struct Hash *
Hash_Clone (const struct Hash *p_pSource) {
    struct Hash *l_pDestination;

    ASSERT (p_pSource != NULL);
    if (p_pSource == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: p_pSource is NULL", __func__);
        return NULL;
    }

    if ((p_pSource->iFlags & HASH_FLAG_FINAL) != HASH_FLAG_FINAL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Can not copy a non finalized hash", __func__);
        return NULL;
    }

    if ((l_pDestination = (struct Hash *) calloc(1, sizeof(struct Hash))) == NULL) {
        return NULL;
    }

    l_pDestination->iType = p_pSource->iType;
    l_pDestination->iSize = p_pSource->iSize;
    l_pDestination->iFlags = HASH_FLAG_FINAL;

    if ((l_pDestination->pData = (uint8_t *) malloc(p_pSource->iSize)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocation new data: %i bytes", __func__, p_pSource->iSize);
        Hash_Destroy(l_pDestination);
        return NULL;
    }

    memcpy(l_pDestination->pData, p_pSource->pData, p_pSource->iSize);

    return l_pDestination;
}


