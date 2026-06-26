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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "config.h"

#include <razorback/debug.h>
#include <razorback/fileserver.h>
#include <razorback/hash.h>
#include <razorback/log.h>
#include <razorback/block_pool.h>

#include <curl/curl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef P_tmpdir
#define P_tmpdir "/tmp"
#endif

#define DEFAULT_FILESERVER_URL "http://file-server:8080"
#define DEFAULT_FILESERVER_TIMEOUT 30U
#define MAX_FILESERVER_ATTEMPTS 10

struct RzbNextFileserverClient
{
    char *baseUrl;
    uint64_t fetchTimeoutSeconds;
    uint64_t uploadTimeoutSeconds;
};

struct ResponseBuffer
{
    char *memory;
    size_t size;
};

struct BlockPoolUpload
{
    struct BlockPoolItem *item;
    struct BlockPoolData *dataItem;
    size_t bytesRead;
    size_t bytesTransferred;
};

static const char *Fileserver_DefaultBaseUrl(void);
static uint64_t Fileserver_DefaultTimeout(const char *primaryName,
                                          const char *legacyName);
static char *Fileserver_CopyBaseUrl(const char *baseUrl);
static enum RzbNextFileserverStatus Fileserver_BlockFilename(
    const struct BlockId *blockId,
    char **filename
);
static bool Fileserver_IsSha256Block(const struct BlockId *blockId);
static bool Fileserver_IsSuccess(long httpCode);
static bool Fileserver_IsRetryableHttp(long httpCode);
static enum RzbNextFileserverStatus Fileserver_StatusForHttp(long httpCode,
                                                             const char *body);
static size_t Fileserver_WriteMemory(void *contents, size_t size, size_t nmemb,
                                     void *userp);
static size_t Fileserver_WriteFile(void *contents, size_t size, size_t nmemb,
                                   void *userp);
static size_t Fileserver_ReadBlockPool(char *buffer, size_t size, size_t nitems,
                                       void *userdata);
static enum RzbNextFileserverStatus Fileserver_StoreMime(
    RzbNextFileserverClient_t *client,
    const struct BlockId *blockId,
    bool (*configurePart)(curl_mimepart *part, void *userData),
    void *userData
);
static enum RzbNextFileserverStatus Fileserver_PerformWithRetry(
    enum RzbNextFileserverStatus (*operation)(void *userData),
    void *userData
);
static enum RzbNextFileserverStatus Fileserver_VerifyFetchedFile(
    const struct BlockId *blockId,
    FILE *file
);
static enum RzbNextFileserverStatus Fileserver_VerifyBytes(
    const struct BlockId *blockId,
    const uint8_t *data,
    size_t length
);
static enum RzbNextFileserverStatus Fileserver_VerifyFile(
    const struct BlockId *blockId,
    const char *fileName
);
static enum RzbNextFileserverStatus Fileserver_VerifyBlockPoolItem(
    const struct BlockPoolItem *item
);
static bool Fileserver_UpdateHashBytes(struct Hash *hash, const uint8_t *data,
                                       size_t length);
static bool Fileserver_ConfigFilePart(curl_mimepart *part, void *userData);
static bool Fileserver_ConfigBytesPart(curl_mimepart *part, void *userData);
static bool Fileserver_ConfigBlockPoolPart(curl_mimepart *part, void *userData);
static void Fileserver_RewindBlockPoolData(struct BlockPoolData *dataItem);

struct BytesUpload
{
    const uint8_t *data;
    size_t length;
};

struct StoreMimeRequest
{
    RzbNextFileserverClient_t *client;
    const struct BlockId *blockId;
    bool (*configurePart)(curl_mimepart *part, void *userData);
    void *userData;
};

struct FetchRequest
{
    RzbNextFileserverClient_t *client;
    const struct BlockId *blockId;
    FILE *file;
    char *fileName;
    size_t bytesTransferred;
};

static const char *
Fileserver_DefaultBaseUrl(void)
{
    const char *value;

    value = getenv("RZB_FILESERVER__URL");
    if (value != NULL && value[0] != '\0')
        return value;
    value = getenv("RZB_FILESERVER_URL");
    if (value != NULL && value[0] != '\0')
        return value;
    return DEFAULT_FILESERVER_URL;
}

static uint64_t
Fileserver_DefaultTimeout(const char *primaryName, const char *legacyName)
{
    const char *value;
    char *end = NULL;
    unsigned long long parsed;

    value = getenv(primaryName);
    if ((value == NULL || value[0] == '\0') && legacyName != NULL)
        value = getenv(legacyName);
    if (value == NULL || value[0] == '\0')
        return DEFAULT_FILESERVER_TIMEOUT;
    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed == 0)
        return DEFAULT_FILESERVER_TIMEOUT;
    return (uint64_t)parsed;
}

static char *
Fileserver_CopyBaseUrl(const char *baseUrl)
{
    char *copy;
    size_t length;

    if (baseUrl == NULL || baseUrl[0] == '\0')
        baseUrl = Fileserver_DefaultBaseUrl();
    copy = strdup(baseUrl);
    if (copy == NULL)
        return NULL;
    length = strlen(copy);
    while (length > 0 && copy[length - 1] == '/') {
        copy[length - 1] = '\0';
        length--;
    }
    return copy;
}

SO_PUBLIC RzbNextFileserverClient_t *
RzbNextFileserverClient_Create(const char *baseUrl,
                               uint64_t fetchTimeoutSeconds,
                               uint64_t uploadTimeoutSeconds)
{
    RzbNextFileserverClient_t *client;

    client = calloc(1, sizeof(*client));
    if (client == NULL)
        return NULL;
    client->baseUrl = Fileserver_CopyBaseUrl(baseUrl);
    if (client->baseUrl == NULL) {
        free(client);
        return NULL;
    }
    client->fetchTimeoutSeconds = fetchTimeoutSeconds == 0 ?
        Fileserver_DefaultTimeout("RZB_FILESERVER__FETCH_TIMEOUT",
                                  "RZB_FILESERVER_FETCH_TIMEOUT") :
        fetchTimeoutSeconds;
    client->uploadTimeoutSeconds = uploadTimeoutSeconds == 0 ?
        Fileserver_DefaultTimeout("RZB_FILESERVER__UPLOAD_TIMEOUT",
                                  "RZB_FILESERVER_UPLOAD_TIMEOUT") :
        uploadTimeoutSeconds;
    return client;
}

SO_PUBLIC void
RzbNextFileserverClient_Destroy(RzbNextFileserverClient_t *client)
{
    if (client == NULL)
        return;
    free(client->baseUrl);
    free(client);
}

static bool
Fileserver_IsSha256Block(const struct BlockId *blockId)
{
    return blockId != NULL &&
           blockId->pHash != NULL &&
           blockId->pHash->iType == HASH_TYPE_SHA256 &&
           blockId->pHash->iSize == 32 &&
           (blockId->pHash->iFlags & HASH_FLAG_FINAL) != 0 &&
           blockId->iLength > 0;
}

static enum RzbNextFileserverStatus
Fileserver_BlockFilename(const struct BlockId *blockId, char **filename)
{
    char *hashText;

    if (filename == NULL || !Fileserver_IsSha256Block(blockId))
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    hashText = Hash_ToText(blockId->pHash);
    if (hashText == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    if (asprintf(filename, "%s.%ju", hashText, (uintmax_t)blockId->iLength) == -1) {
        free(hashText);
        *filename = NULL;
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    }
    free(hashText);
    return RZB_NEXT_FILESERVER_OK;
}

SO_PUBLIC enum RzbNextFileserverStatus
RzbNextFileserver_BuildUrl(const RzbNextFileserverClient_t *client,
                           const struct BlockId *blockId,
                           char **url)
{
    char *filename = NULL;
    enum RzbNextFileserverStatus status;

    if (client == NULL || client->baseUrl == NULL || url == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    status = Fileserver_BlockFilename(blockId, &filename);
    if (status != RZB_NEXT_FILESERVER_OK)
        return status;
    if (asprintf(url, "%s/%c/%c/%c/%c/%s",
                 client->baseUrl,
                 filename[0],
                 filename[1],
                 filename[2],
                 filename[3],
                 filename) == -1) {
        free(filename);
        *url = NULL;
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    }
    free(filename);
    return RZB_NEXT_FILESERVER_OK;
}

static bool
Fileserver_IsSuccess(long httpCode)
{
    return httpCode >= 200 && httpCode < 300;
}

static bool
Fileserver_IsRetryableHttp(long httpCode)
{
    return httpCode == 408 || httpCode == 429 || httpCode >= 500;
}

static enum RzbNextFileserverStatus
Fileserver_StatusForHttp(long httpCode, const char *body)
{
    if (Fileserver_IsSuccess(httpCode))
        return RZB_NEXT_FILESERVER_OK;
    if (httpCode == 404)
        return RZB_NEXT_FILESERVER_NOT_FOUND;
    if (httpCode == 400 && body != NULL && strstr(body, "already exists") != NULL)
        return RZB_NEXT_FILESERVER_OK;
    if (Fileserver_IsRetryableHttp(httpCode))
        return RZB_NEXT_FILESERVER_RETRYABLE;
    return RZB_NEXT_FILESERVER_HTTP_ERROR;
}

static size_t
Fileserver_WriteMemory(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realSize = size * nmemb;
    struct ResponseBuffer *buffer = userp;
    char *next;

    next = realloc(buffer->memory, buffer->size + realSize + 1);
    if (next == NULL)
        return 0;
    buffer->memory = next;
    memcpy(&(buffer->memory[buffer->size]), contents, realSize);
    buffer->size += realSize;
    buffer->memory[buffer->size] = '\0';
    return realSize;
}

static size_t
Fileserver_WriteFile(void *contents, size_t size, size_t nmemb, void *userp)
{
    struct FetchRequest *request = userp;
    size_t written;

    written = fwrite(contents, size, nmemb, request->file);
    request->bytesTransferred += size * written;
    return written;
}

static size_t
Fileserver_ReadBlockPool(char *buffer, size_t size, size_t nitems, void *userdata)
{
    struct BlockPoolUpload *upload = userdata;
    size_t want = size * nitems;
    size_t read = 0;

    while (read < want && upload->dataItem != NULL) {
        size_t available;
        size_t remaining;
        size_t toRead;

        if (upload->dataItem->iFlags == BLOCK_POOL_DATA_FLAG_FILE) {
            available = upload->dataItem->iLength - upload->bytesRead;
            remaining = want - read;
            toRead = available < remaining ? available : remaining;
            if (toRead == 0) {
                upload->dataItem = upload->dataItem->pNext;
                upload->bytesRead = 0;
                continue;
            }
            toRead = fread(buffer + read, 1, toRead, upload->dataItem->data.file);
        } else {
            available = upload->dataItem->iLength - upload->bytesRead;
            remaining = want - read;
            toRead = available < remaining ? available : remaining;
            if (toRead > 0) {
                memcpy(buffer + read,
                       upload->dataItem->data.pointer + upload->bytesRead,
                       toRead);
            }
        }
        read += toRead;
        upload->bytesRead += toRead;
        upload->bytesTransferred += toRead;
        if (upload->bytesRead >= upload->dataItem->iLength) {
            upload->dataItem = upload->dataItem->pNext;
            upload->bytesRead = 0;
        }
        if (toRead == 0)
            break;
    }
    return read;
}

static enum RzbNextFileserverStatus
Fileserver_PerformWithRetry(enum RzbNextFileserverStatus (*operation)(void *userData),
                            void *userData)
{
    enum RzbNextFileserverStatus status = RZB_NEXT_FILESERVER_LOCAL_ERROR;
    unsigned int delay = 1;
    int attempt;

    for (attempt = 0; attempt < MAX_FILESERVER_ATTEMPTS; attempt++) {
        status = operation(userData);
        if (status != RZB_NEXT_FILESERVER_RETRYABLE)
            return status;
        if (attempt + 1 < MAX_FILESERVER_ATTEMPTS) {
            sleep(delay);
            if (delay < 8U)
                delay *= 2U;
        }
    }
    return status;
}

static enum RzbNextFileserverStatus
Fileserver_StoreMimeAttempt(void *userData)
{
    struct StoreMimeRequest *request = userData;
    struct ResponseBuffer response = { NULL, 0 };
    CURL *curl = NULL;
    curl_mime *mime = NULL;
    curl_mimepart *part = NULL;
    char *url = NULL;
    CURLcode code;
    long httpCode = 0;
    enum RzbNextFileserverStatus status = RZB_NEXT_FILESERVER_LOCAL_ERROR;

    response.memory = calloc(1, sizeof(char));
    if (response.memory == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    status = RzbNextFileserver_BuildUrl(request->client, request->blockId, &url);
    if (status != RZB_NEXT_FILESERVER_OK)
        goto cleanup;
    curl = curl_easy_init();
    if (curl == NULL)
        goto cleanup;
    mime = curl_mime_init(curl);
    if (mime == NULL)
        goto cleanup;
    part = curl_mime_addpart(mime);
    if (part == NULL)
        goto cleanup;
    if (!request->configurePart(part, request->userData))
        goto cleanup;
    if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,
                         (long)request->client->uploadTimeoutSeconds) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Fileserver_WriteMemory) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime) != CURLE_OK) {
        goto cleanup;
    }
    code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        status = RZB_NEXT_FILESERVER_RETRYABLE;
        goto cleanup;
    }
    if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode) != CURLE_OK)
        goto cleanup;
    status = Fileserver_StatusForHttp(httpCode, response.memory);

cleanup:
    if (mime != NULL)
        curl_mime_free(mime);
    if (curl != NULL)
        curl_easy_cleanup(curl);
    free(response.memory);
    free(url);
    return status;
}

static enum RzbNextFileserverStatus
Fileserver_StoreMime(RzbNextFileserverClient_t *client,
                     const struct BlockId *blockId,
                     bool (*configurePart)(curl_mimepart *part, void *userData),
                     void *userData)
{
    struct StoreMimeRequest request = {
        client,
        blockId,
        configurePart,
        userData
    };

    if (client == NULL || blockId == NULL || configurePart == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    return Fileserver_PerformWithRetry(Fileserver_StoreMimeAttempt, &request);
}

static bool
Fileserver_ConfigFilePart(curl_mimepart *part, void *userData)
{
    const char *fileName = userData;

    return curl_mime_name(part, "file") == CURLE_OK &&
           curl_mime_filename(part, "file") == CURLE_OK &&
           curl_mime_type(part, "application/octet-stream") == CURLE_OK &&
           curl_mime_filedata(part, fileName) == CURLE_OK;
}

static bool
Fileserver_ConfigBytesPart(curl_mimepart *part, void *userData)
{
    const struct BytesUpload *upload = userData;

    return curl_mime_name(part, "file") == CURLE_OK &&
           curl_mime_filename(part, "file") == CURLE_OK &&
           curl_mime_type(part, "application/octet-stream") == CURLE_OK &&
           curl_mime_data(part, (const char *)upload->data,
                          upload->length) == CURLE_OK;
}

static bool
Fileserver_ConfigBlockPoolPart(curl_mimepart *part, void *userData)
{
    struct BlockPoolUpload *upload = userData;
    curl_off_t length;

    if (upload == NULL || upload->item == NULL || upload->item->pEvent == NULL ||
        upload->item->pEvent->pBlock == NULL ||
        upload->item->pEvent->pBlock->pId == NULL) {
        return false;
    }
    upload->dataItem = upload->item->pDataHead;
    upload->bytesRead = 0;
    upload->bytesTransferred = 0;
    Fileserver_RewindBlockPoolData(upload->dataItem);
    length = (curl_off_t)upload->item->pEvent->pBlock->pId->iLength;
    return curl_mime_name(part, "file") == CURLE_OK &&
           curl_mime_filename(part, "file") == CURLE_OK &&
           curl_mime_type(part, "application/octet-stream") == CURLE_OK &&
           curl_mime_data_cb(part, length, Fileserver_ReadBlockPool,
                             NULL, NULL, upload) == CURLE_OK;
}

static void
Fileserver_RewindBlockPoolData(struct BlockPoolData *dataItem)
{
    while (dataItem != NULL) {
        if (dataItem->iFlags == BLOCK_POOL_DATA_FLAG_FILE &&
            dataItem->data.file != NULL) {
            rewind(dataItem->data.file);
        }
        dataItem = dataItem->pNext;
    }
}

SO_PUBLIC enum RzbNextFileserverStatus
RzbNextFileserver_StoreFile(RzbNextFileserverClient_t *client,
                            const struct BlockId *blockId,
                            const char *fileName)
{
    enum RzbNextFileserverStatus status;

    if (fileName == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    status = Fileserver_VerifyFile(blockId, fileName);
    if (status != RZB_NEXT_FILESERVER_OK)
        return status;
    return Fileserver_StoreMime(client, blockId, Fileserver_ConfigFilePart,
                                (void *)fileName);
}

SO_PUBLIC enum RzbNextFileserverStatus
RzbNextFileserver_StoreBytes(RzbNextFileserverClient_t *client,
                             const struct BlockId *blockId,
                             const uint8_t *data,
                             size_t length)
{
    struct BytesUpload upload = { data, length };
    enum RzbNextFileserverStatus status;

    if (data == NULL || length == 0)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    status = Fileserver_VerifyBytes(blockId, data, length);
    if (status != RZB_NEXT_FILESERVER_OK)
        return status;
    return Fileserver_StoreMime(client, blockId, Fileserver_ConfigBytesPart,
                                &upload);
}

SO_PUBLIC enum RzbNextFileserverStatus
RzbNextFileserver_StoreBlockPoolItem(RzbNextFileserverClient_t *client,
                                     struct BlockPoolItem *item)
{
    struct BlockPoolUpload upload = { item, NULL, 0, 0 };
    enum RzbNextFileserverStatus status;

    if (item == NULL || item->pEvent == NULL || item->pEvent->pBlock == NULL ||
        item->pEvent->pBlock->pId == NULL || item->pDataHead == NULL) {
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    }
    status = Fileserver_VerifyBlockPoolItem(item);
    if (status != RZB_NEXT_FILESERVER_OK)
        return status;
    status = Fileserver_StoreMime(client, item->pEvent->pBlock->pId,
                                  Fileserver_ConfigBlockPoolPart, &upload);
    Fileserver_RewindBlockPoolData(item->pDataHead);
    return status;
}

static enum RzbNextFileserverStatus
Fileserver_FetchAttempt(void *userData)
{
    struct FetchRequest *request = userData;
    CURL *curl = NULL;
    char *url = NULL;
    CURLcode code;
    long httpCode = 0;
    enum RzbNextFileserverStatus status = RZB_NEXT_FILESERVER_LOCAL_ERROR;

    request->bytesTransferred = 0;
    rewind(request->file);
    if (ftruncate(fileno(request->file), 0) != 0)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    status = RzbNextFileserver_BuildUrl(request->client, request->blockId, &url);
    if (status != RZB_NEXT_FILESERVER_OK)
        return status;
    curl = curl_easy_init();
    if (curl == NULL)
        goto cleanup;
    if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,
                         (long)request->client->fetchTimeoutSeconds) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Fileserver_WriteFile) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, request) != CURLE_OK) {
        goto cleanup;
    }
    code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        status = RZB_NEXT_FILESERVER_RETRYABLE;
        goto cleanup;
    }
    if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode) != CURLE_OK)
        goto cleanup;
    status = Fileserver_StatusForHttp(httpCode, NULL);
    if (status == RZB_NEXT_FILESERVER_OK)
        status = Fileserver_VerifyFetchedFile(request->blockId, request->file);

cleanup:
    if (curl != NULL)
        curl_easy_cleanup(curl);
    free(url);
    return status;
}

static bool
Fileserver_UpdateHashBytes(struct Hash *hash, const uint8_t *data,
                           size_t length)
{
    size_t offset = 0;

    while (offset < length) {
        size_t chunk = length - offset;
        if (chunk > UINT32_MAX)
            chunk = UINT32_MAX;
        if (!Hash_Update(hash, (uint8_t *)(data + offset), (uint32_t)chunk))
            return false;
        offset += chunk;
    }
    return true;
}

static enum RzbNextFileserverStatus
Fileserver_VerifyBytes(const struct BlockId *blockId,
                       const uint8_t *data,
                       size_t length)
{
    struct Hash *hash;
    bool success;

    if (!Fileserver_IsSha256Block(blockId) || data == NULL || length == 0)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    if ((uint64_t)length != blockId->iLength)
        return RZB_NEXT_FILESERVER_PAYLOAD_MISMATCH;
    hash = Hash_Create_Type(HASH_TYPE_SHA256);
    if (hash == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    success = Fileserver_UpdateHashBytes(hash, data, length) &&
              Hash_Finalize(hash) &&
              Hash_IsEqual(hash, blockId->pHash);
    Hash_Destroy(hash);
    return success ? RZB_NEXT_FILESERVER_OK :
                     RZB_NEXT_FILESERVER_PAYLOAD_MISMATCH;
}

static enum RzbNextFileserverStatus
Fileserver_VerifyFile(const struct BlockId *blockId,
                      const char *fileName)
{
    FILE *file;
    enum RzbNextFileserverStatus status;

    if (!Fileserver_IsSha256Block(blockId) || fileName == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    file = fopen(fileName, "rb");
    if (file == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    status = Fileserver_VerifyFetchedFile(blockId, file);
    fclose(file);
    return status;
}

static enum RzbNextFileserverStatus
Fileserver_VerifyBlockPoolItem(const struct BlockPoolItem *item)
{
    const struct BlockId *blockId;
    const struct BlockPoolData *dataItem;
    struct Hash *hash;
    uint64_t total = 0;
    bool success = true;
    uint8_t buffer[8192];

    if (item == NULL || item->pEvent == NULL || item->pEvent->pBlock == NULL ||
        item->pEvent->pBlock->pId == NULL || item->pDataHead == NULL) {
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    }
    blockId = item->pEvent->pBlock->pId;
    if (!Fileserver_IsSha256Block(blockId))
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    hash = Hash_Create_Type(HASH_TYPE_SHA256);
    if (hash == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    for (dataItem = item->pDataHead; dataItem != NULL && success;
         dataItem = dataItem->pNext) {
        total += dataItem->iLength;
        if (dataItem->iFlags == BLOCK_POOL_DATA_FLAG_FILE) {
            size_t readBytes;

            if (dataItem->data.file == NULL) {
                success = false;
                break;
            }
            rewind(dataItem->data.file);
            while ((readBytes = fread(buffer, 1, sizeof(buffer),
                                      dataItem->data.file)) > 0) {
                if (!Fileserver_UpdateHashBytes(hash, buffer, readBytes)) {
                    success = false;
                    break;
                }
            }
            if (ferror(dataItem->data.file))
                success = false;
            rewind(dataItem->data.file);
        } else {
            if (dataItem->data.pointer == NULL ||
                !Fileserver_UpdateHashBytes(hash, dataItem->data.pointer,
                                            dataItem->iLength)) {
                success = false;
            }
        }
    }
    success = success && total == blockId->iLength && Hash_Finalize(hash) &&
              Hash_IsEqual(hash, blockId->pHash);
    Hash_Destroy(hash);
    Fileserver_RewindBlockPoolData(item->pDataHead);
    return success ? RZB_NEXT_FILESERVER_OK :
                     RZB_NEXT_FILESERVER_PAYLOAD_MISMATCH;
}

SO_PUBLIC enum RzbNextFileserverStatus
RzbNextFileserver_FetchToFile(RzbNextFileserverClient_t *client,
                              const struct BlockId *blockId,
                              char **fileName)
{
    struct FetchRequest request = { client, blockId, NULL, NULL, 0 };
    const char *tempDir;
    int fd;
    enum RzbNextFileserverStatus status;

    if (client == NULL || blockId == NULL || fileName == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    tempDir = getenv("TMPDIR");
    if (tempDir == NULL || tempDir[0] == '\0')
        tempDir = P_tmpdir;
    if (asprintf(&request.fileName, "%s/rzb-XXXXXX", tempDir) == -1)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    fd = mkstemp(request.fileName);
    if (fd == -1) {
        free(request.fileName);
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    }
    request.file = fdopen(fd, "w+b");
    if (request.file == NULL) {
        close(fd);
        unlink(request.fileName);
        free(request.fileName);
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    }
    status = Fileserver_PerformWithRetry(Fileserver_FetchAttempt, &request);
    fclose(request.file);
    if (status != RZB_NEXT_FILESERVER_OK) {
        unlink(request.fileName);
        free(request.fileName);
        return status;
    }
    *fileName = request.fileName;
    return RZB_NEXT_FILESERVER_OK;
}

static enum RzbNextFileserverStatus
Fileserver_VerifyFetchedFile(const struct BlockId *blockId, FILE *file)
{
    struct Hash *hash = NULL;
    bool success;
    off_t length;

    if (blockId == NULL || file == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    if (fflush(file) != 0)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    if (fseeko(file, 0, SEEK_END) != 0)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    length = ftello(file);
    if (length < 0)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    if ((uint64_t)length != blockId->iLength)
        return RZB_NEXT_FILESERVER_PAYLOAD_MISMATCH;
    if (fseeko(file, 0, SEEK_SET) != 0)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    hash = Hash_Create_Type(HASH_TYPE_SHA256);
    if (hash == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    success = Hash_Update_File(hash, file) && Hash_Finalize(hash) &&
              Hash_IsEqual(hash, blockId->pHash);
    Hash_Destroy(hash);
    if (fseeko(file, 0, SEEK_SET) != 0)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    return success ? RZB_NEXT_FILESERVER_OK :
                     RZB_NEXT_FILESERVER_PAYLOAD_MISMATCH;
}

SO_PUBLIC bool
RzbNextFileserver_AttachFileToBlock(struct Block *block, char *fileName,
                                    bool tempFile)
{
    ASSERT(block != NULL);
    ASSERT(fileName != NULL);
    if (block == NULL || fileName == NULL)
        return false;
    block->data.file = fopen(fileName, "rb");
    if (block->data.file == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: failed to open block file %s: %s",
                __func__, fileName, strerror(errno));
        return false;
    }
    block->data.fileName = fileName;
    block->data.tempFile = tempFile;
    block->data.pointer = mmap(NULL, block->pId->iLength, PROT_READ, MAP_PRIVATE,
                               fileno(block->data.file), 0);
    if (block->data.pointer == MAP_FAILED) {
        block->data.pointer = NULL;
        fclose(block->data.file);
        block->data.file = NULL;
        return false;
    }
    return true;
}

SO_PUBLIC void
RzbNextFileserver_FreeBlockData(struct Block *block)
{
    if (block == NULL)
        return;
    if (block->data.pointer != NULL) {
        munmap(block->data.pointer, block->pId->iLength);
        block->data.pointer = NULL;
    }
    if (block->data.file != NULL) {
        fclose(block->data.file);
        block->data.file = NULL;
    }
    if (block->data.tempFile && block->data.fileName != NULL)
        unlink(block->data.fileName);
    free(block->data.fileName);
    block->data.fileName = NULL;
    block->data.tempFile = false;
}

SO_PUBLIC enum RzbNextFileserverStatus
RzbNextFileserver_FetchBlock(RzbNextFileserverClient_t *client,
                             struct Block *block)
{
    char *fileName = NULL;
    enum RzbNextFileserverStatus status;

    if (block == NULL || block->pId == NULL)
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    status = RzbNextFileserver_FetchToFile(client, block->pId, &fileName);
    if (status != RZB_NEXT_FILESERVER_OK)
        return status;
    if (!RzbNextFileserver_AttachFileToBlock(block, fileName, true)) {
        unlink(fileName);
        free(fileName);
        return RZB_NEXT_FILESERVER_LOCAL_ERROR;
    }
    return RZB_NEXT_FILESERVER_OK;
}
