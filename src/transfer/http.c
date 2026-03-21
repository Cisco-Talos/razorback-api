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
#include <razorback/types.h>
#include <razorback/list.h>

#include <razorback/log.h>
#include <razorback/hash.h>
#include <razorback/block_pool.h>
#include <razorback/telemetry.h>
#include <razorback/transfer.h>

#include <curl/curl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "transfer/core.h"
#include "runtime_config.h"
#include "../telemetry.h"

#ifndef P_tmpdir
#define P_tmpdir "/tmp"
#endif

static bool sg_bSkipStore = false;

static struct TransportDescriptor descriptorHttp = {
    TRANSFER_MODE_HTTP,
    "HTTP",
    "Transfer file via HTTP",
    Transfer_HTTP_Store,
    Transfer_HTTP_Fetch
};

static struct TransportDescriptor descriptorHttps = {
    TRANSFER_MODE_HTTPS,
    "HTTPS",
    "Transfer file via HTTPS",
    Transfer_HTTP_Store,
    Transfer_HTTP_Fetch
};

bool HTTP_Init(void)
{
    const char *skip = getenv("RZB_SKIP_HTTP_STORE");
    if (skip != NULL)
    {
        if (strcmp(skip, "1") == 0 || strcasecmp(skip, "true") == 0)
        {
            sg_bSkipStore = true;
            rzb_log(LOG_INFO,LOG_C_TRANSFER, "%s: HTTP Store disabled via RZB_SKIP_HTTP_STORE", __func__);
        }
    }
    return Transport_Register(&descriptorHttp) && Transport_Register(&descriptorHttps);
}

struct StoreContext
{
    struct BlockPoolItem *item;
    struct BlockPoolData *dataItem;
    size_t bytesRead;
    size_t bytesTransferred;
    char * filename;
    uint8_t protocol;
    uint16_t port;
    enum TransferStatus status;
    char * memory;
    size_t size;
};

static const char *
HTTP_GetTempDirectory(void)
{
    const char *path = Config_getLocalityBlockStore();

    if (path != NULL && path[0] != '\0' && !Config_isBlockStoreRemote()) {
        return path;
    }

    path = getenv("TMPDIR");
    if (path != NULL && path[0] != '\0') {
        return path;
    }

    return P_tmpdir;
}

static bool
HTTP_GetProtocolSettings(uint8_t protocol, const char **scheme, bool *secure,
                         const char *caller)
{
    ASSERT(scheme != NULL);
    ASSERT(secure != NULL);
    if (scheme == NULL || secure == NULL) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: invalid protocol settings output", caller);
        return false;
    }

    switch (protocol) {
    case TRANSFER_MODE_HTTP:
        *scheme = "http";
        *secure = false;
        return true;
    case TRANSFER_MODE_HTTPS:
        *scheme = "https";
        *secure = true;
        return true;
    default:
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: unsupported HTTP transport protocol: %u",
                caller, protocol);
        return false;
    }
}

static bool
HTTP_BuildURL(char **url, uint8_t protocol, const char *address, uint16_t port,
              const char *filename, const char *caller)
{
    size_t filenameLength;
    const char *scheme;
    bool secure;

    ASSERT(url != NULL);
    ASSERT(address != NULL);
    ASSERT(filename != NULL);
    if (url == NULL || address == NULL || filename == NULL) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: invalid URL parameters", caller);
        return false;
    }
    if (!HTTP_GetProtocolSettings(protocol, &scheme, &secure, caller)) {
        return false;
    }
    (void)secure;

    filenameLength = strlen(filename);
    if (filenameLength < 4) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: invalid transfer filename", caller);
        return false;
    }

    if (asprintf(url, "%s://%s:%u/%c/%c/%c/%c/%s",
                 scheme,
                 address,
                 port,
                 filename[0],
                 filename[1],
                 filename[2],
                 filename[3],
                 filename) == -1) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: Failed to generate URL", caller);
        return false;
    }

    return true;
}

static bool
HTTP_ConfigureCurl(CURL *curl, uint8_t protocol, const char *url)
{
    bool secure;
    const char *scheme;

    ASSERT(curl != NULL);
    ASSERT(url != NULL);
    if (curl == NULL || url == NULL) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: invalid CURL parameters", __func__);
        return false;
    }
    if (!HTTP_GetProtocolSettings(protocol, &scheme, &secure, __func__)) {
        return false;
    }

    if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: Failed to configure %s connection",
                __func__, scheme);
        return false;
    }
    if (secure &&
        ((curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2) != CURLE_OK) ||
         (curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L) != CURLE_OK) ||
         (curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L) != CURLE_OK))) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: Failed to configure %s connection",
                __func__, scheme);
        return false;
    }

    return true;
}

static void
HTTP_AddCommonTelemetryAttributes(TelemetrySpan_t *span,
                                  const struct Block *block,
                                  uint8_t protocol,
                                  const char *address,
                                  uint16_t port,
                                  const char *url,
                                  const char *method)
{
    const char *scheme = NULL;
    bool secure = false;

    if (span == NULL)
        return;

    if (block != NULL)
        Telemetry_AddBlockAttributes(span, block);

    if (method != NULL && method[0] != '\0')
        Telemetry_AddStringAttribute(span, "http.request.method", method);

    if (address != NULL && address[0] != '\0')
        Telemetry_AddStringAttribute(span, "server.address", address);

    Telemetry_AddIntAttribute(span, "server.port", (int64_t)port);

    if (url != NULL && url[0] != '\0')
        Telemetry_AddStringAttribute(span, "url.full", url);

    if (HTTP_GetProtocolSettings(protocol, &scheme, &secure, __func__))
        Telemetry_AddStringAttribute(span, "url.scheme", scheme);
}

static void
HTTP_AddErrorTypeAttribute(TelemetrySpan_t *span,
                           const char *errorType)
{
    if (span == NULL || errorType == NULL || errorType[0] == '\0')
        return;

    Telemetry_AddStringAttribute(span, "error.type", errorType);
}

static void
HTTP_AddStatusCodeErrorType(TelemetrySpan_t *span,
                            long httpCode)
{
    char errorType[32];

    if (span == NULL || httpCode <= 0)
        return;

    if (snprintf(errorType, sizeof(errorType), "%ld", httpCode) < 0)
        return;

    Telemetry_AddStringAttribute(span, "error.type", errorType);
}

static void
HTTP_FreeRequestHeaders(struct curl_slist **requestHeaders)
{
    if (requestHeaders == NULL || *requestHeaders == NULL)
        return;

    curl_slist_free_all(*requestHeaders);
    *requestHeaders = NULL;
}

static bool
HTTP_ApplyTelemetryHeaders(CURL *curl,
                           struct curl_slist **requestHeaders)
{
    struct TelemetryInjectedHeaders injectedHeaders = { 0, NULL };
    size_t i;

    if (curl == NULL || requestHeaders == NULL)
        return false;

    if (!Telemetry_InjectCurrentContext(&injectedHeaders))
        return false;

    for (i = 0; i < injectedHeaders.count; ++i) {
        char *headerLine = NULL;
        struct curl_slist *nextHeaders;

        if (injectedHeaders.entries[i].name == NULL ||
                injectedHeaders.entries[i].value == NULL) {
            continue;
        }

        if (asprintf(&headerLine, "%s: %s",
                     injectedHeaders.entries[i].name,
                     injectedHeaders.entries[i].value) == -1) {
            Telemetry_FreeInjectedHeaders(&injectedHeaders);
            HTTP_FreeRequestHeaders(requestHeaders);
            return false;
        }

        nextHeaders = curl_slist_append(*requestHeaders, headerLine);
        free(headerLine);
        if (nextHeaders == NULL) {
            Telemetry_FreeInjectedHeaders(&injectedHeaders);
            HTTP_FreeRequestHeaders(requestHeaders);
            return false;
        }

        *requestHeaders = nextHeaders;
    }

    Telemetry_FreeInjectedHeaders(&injectedHeaders);

    if (*requestHeaders == NULL)
        return true;

    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *requestHeaders) != CURLE_OK) {
        HTTP_FreeRequestHeaders(requestHeaders);
        return false;
    }

    return true;
}

static size_t
read_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    struct StoreContext *context = (struct StoreContext *)userdata;
    //rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Reading %zu blocks of size %zu", __func__, nitems, size);
    if (context->dataItem == NULL) {
       // rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: No more data to read", __func__);
        return 0;
    }
    if (context->dataItem->iFlags == BLOCK_POOL_DATA_FLAG_FILE) {
        // If the data is a file, read from the file handle
        if (context->bytesRead + size * nitems > context->dataItem->iLength) {
            // If we are trying to read more data than is available in the file
            nitems = (context->dataItem->iLength - context->bytesRead);
            //rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Read too much data from file, read truncated: %zu", __func__, nitems);
        }
        {
            size_t itemsRead = fread(buffer, size, nitems,
                                     context->dataItem->data.file);
            size_t fileBytesRead = size * itemsRead;

            context->bytesRead += fileBytesRead;
            context->bytesTransferred += fileBytesRead;
            return itemsRead;
        }
    } else {

        size_t want = size * nitems;
        size_t read = 0;
        while (read < want ) {
            //rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Want %zu bytes, read %zu bytes", __func__, want, read);
            // If we have run out of buffers just return the requested size as to not read random data
            if (context->dataItem == NULL) {
              //  rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: No more data to read", __func__);
                return read;
            }
            // How much data is left to read in the current buffer
            //rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Current buffer length %zu, bytes read %zu", __func__, context->dataItem->iLength, context->bytesRead);
            size_t avail = context->dataItem->iLength - context->bytesRead;
            size_t remain = want - read;
            // If more data is wanted that in the current buffer then only
            // copy the available data in the current buffer
            size_t to_read = avail < want ? avail : want;
            to_read = remain < to_read ? remain : to_read;
            //rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Reading %zu bytes from buffer - avail %zu - want %zu", __func__, to_read, avail, want);
            memcpy(buffer+read, context->dataItem->data.pointer + context->bytesRead, to_read);
            read += to_read;
            context->bytesRead += to_read;
            context->bytesTransferred += to_read;
            if (context->bytesRead == context->dataItem->iLength) {
                // If we have read all the data in the current buffer then
                // move to the next buffer
                context->dataItem = context->dataItem->pNext;
                context->bytesRead = 0;
            }
        }
        return read;
    }
}


static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct StoreContext *mem = (struct StoreContext *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        /* out of memory! */
        rzb_log(LOG_ERR,LOG_C_TRANSFER,"%s: not enough memory (realloc returned NULL)", __func__ );
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static int
HTTP_Try_Store(void *i, void*ud)
{
    struct StoreContext *status = ud;
    char *address = i;
    char *url = NULL;
    CURL *curl = NULL;
    curl_mime *mime = NULL;
    curl_mimepart *part = NULL;
    struct curl_slist *requestHeaders = NULL;
    TelemetrySpan_t *requestSpan = NULL;
    CURLcode res;
    long http_code = 0;
    const char *requestError = NULL;
    bool spanSuccess = false;

    // Reset all the context states incase this is a retry
    status->dataItem = status->item->pDataHead;
    status->bytesRead = 0;
    status->bytesTransferred = 0;
    if (status->dataItem->iFlags == BLOCK_POOL_DATA_FLAG_FILE) {
        rewind(status->dataItem->data.file);
    }

    if (!HTTP_BuildURL(&url, status->protocol, address, status->port, status->filename,
                       __func__)) {
        status->status = TRANSFER_FAIL_LOCAL;
        return LIST_EACH_OK;
    }
    requestSpan = Telemetry_StartSpanWithKind("POST",
                                              NULL,
                                              TELEMETRY_SPAN_KIND_CLIENT);
    HTTP_AddCommonTelemetryAttributes(
        requestSpan,
        (status->item != NULL && status->item->pEvent != NULL) ?
            status->item->pEvent->pBlock : NULL,
        status->protocol,
        address,
        status->port,
        url,
        "POST");
    Telemetry_AddIntAttribute(requestSpan, "rzb.transfer.bytes_expected",
                              (int64_t)status->item->pEvent->pBlock->pId->iLength);
    rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Attempting to store %s at %s", __func__, status->filename, url);
    curl = curl_easy_init();
    if (curl == NULL) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to initialize curl", __func__);
        HTTP_AddErrorTypeAttribute(requestSpan, "curl.init");
        requestError = "failed to initialize curl";
        status->status = TRANSFER_FAIL_LOCAL;
        goto cleanup;
    }
    if (!HTTP_ApplyTelemetryHeaders(curl, &requestHeaders)) {
        rzb_log(LOG_WARNING, LOG_C_TRANSFER,
                "%s: Failed to apply telemetry HTTP headers", __func__);
    }
    mime = curl_mime_init(curl);
    if (mime == NULL) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to initialize curl MIME data", __func__);
        HTTP_AddErrorTypeAttribute(requestSpan, "curl.mime_init");
        requestError = "failed to initialize curl MIME data";
        status->status = TRANSFER_FAIL_LOCAL;
        goto cleanup;
    }
    part = curl_mime_addpart(mime);
    if (part == NULL) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to create curl MIME part", __func__);
        HTTP_AddErrorTypeAttribute(requestSpan, "curl.mime_addpart");
        requestError = "failed to create curl MIME part";
        status->status = TRANSFER_FAIL_LOCAL;
        goto cleanup;
    }

    if (!HTTP_ConfigureCurl(curl, status->protocol, url) ||
            (curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback) != CURLE_OK) ||
            (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback) != CURLE_OK) ||
            (curl_easy_setopt(curl, CURLOPT_WRITEDATA, status) != CURLE_OK) ||
            (curl_mime_name(part, "file") != CURLE_OK) ||
            (curl_mime_filename(part, "file") != CURLE_OK) ||
            (curl_mime_type(part, "application/octet-stream") != CURLE_OK) ||
            (curl_mime_data_cb(part,
                               status->item->pEvent->pBlock->pId->iLength,
                               read_callback,
                               NULL,
                               NULL,
                               status) != CURLE_OK) ||
            (curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime) != CURLE_OK)
        )
    {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to configure HTTP transport request",
                __func__);
        HTTP_AddErrorTypeAttribute(requestSpan, "curl.request_config");
        requestError = "failed to configure HTTP transport request";
        status->status = TRANSFER_FAIL_LOCAL;
        goto cleanup;
    }

    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: curl_easy_perform() failed: %s", __func__,
                curl_easy_strerror(res));
        HTTP_AddErrorTypeAttribute(requestSpan, "curl.perform");
        requestError = curl_easy_strerror(res);
        status->status = TRANSFER_FAIL_DISPATCHER;
    }
    if (curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code) != CURLE_OK) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to get response code", __func__);
        HTTP_AddErrorTypeAttribute(requestSpan, "curl.response_code");
        requestError = "failed to get HTTP response code";
        status->status = TRANSFER_FAIL_LOCAL;
    }

cleanup:
    if (http_code > 0)
        Telemetry_AddIntAttribute(requestSpan, "http.response.status_code",
                                  (int64_t)http_code);
    Telemetry_AddIntAttribute(requestSpan, "rzb.transfer.bytes_transferred",
                              (int64_t)status->bytesTransferred);
    if (mime != NULL) {
        curl_mime_free(mime);
    }
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
    HTTP_FreeRequestHeaders(&requestHeaders);
    free(url);
    // Rewind the filehandle after the request
    if (status->dataItem != NULL && status->dataItem->iFlags == BLOCK_POOL_DATA_FLAG_FILE)
    {
        rewind(status->dataItem->data.file);
    }
    if (status->status != TRANSFER_OK && status->status != TRANSFER_FAIL_DISPATCHER &&
            status->status != TRANSFER_FAIL_LOCAL) {
        status->status = TRANSFER_FAIL_LOCAL;
    }
    if (status->status == TRANSFER_FAIL_LOCAL) {
        if (requestError == NULL)
            requestError = "HTTP store request failed locally";
        Telemetry_EndSpan(requestSpan, false, requestError);
        return LIST_EACH_OK;
    }
    if (http_code != 200) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to store file: %zi", __func__, http_code);
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to store file: %s", __func__, status->memory);
        status->status = TRANSFER_FAIL_DISPATCHER;
        spanSuccess = (http_code >= 100 && http_code < 400);
        if (!spanSuccess)
            HTTP_AddStatusCodeErrorType(requestSpan, http_code);
        Telemetry_EndSpan(requestSpan, spanSuccess, spanSuccess ? NULL : requestError);
        return LIST_EACH_OK;
    }

    rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Successfully stored file", __func__);
    status->status = TRANSFER_OK;
    Telemetry_EndSpan(requestSpan, true, NULL);
    return LIST_EACH_END;
}

static int
HTTP_Store(void *i, void*ud) {
    int try = 0;
    int max_tries = 10;
    struct StoreContext *status = ud;
    int res;
    for (try = 0; try < max_tries; try++) {
        res = HTTP_Try_Store(i, ud);
        if (status->status == TRANSFER_OK) {
            return res;
        }
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to store file, retrying %d/%d", __func__, try+1, max_tries);
    }
    return res;
}

SO_PUBLIC enum TransferStatus
Transfer_HTTP_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher)
{
    ASSERT(item != NULL);
    ASSERT(dispatcher != NULL);
    if (item == NULL || dispatcher == NULL) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: item or dispatcher is NULL", __func__);
        return TRANSFER_FAIL_LOCAL;
    }

    struct StoreContext context = {
        .item = item,
        .dataItem = item->pDataHead,
        .bytesRead = 0,
        .bytesTransferred = 0,
        .filename = NULL,
        .protocol = dispatcher->dispatcher->protocol,
        .port = dispatcher->dispatcher->port,
        .status = TRANSFER_FAIL_LOCAL,
        .memory = NULL,
        .size = 0,
    };
    if (sg_bSkipStore) {
        return TRANSFER_OK;
    }
    if ((context.memory = calloc(1, sizeof(char))) == NULL) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: failed to allocate response buffer", __func__);
        return TRANSFER_FAIL_LOCAL;
    }

    if ((context.filename = Transfer_generateFilename (item->pEvent->pBlock)) == NULL)
    {
        rzb_log (LOG_ERR,LOG_C_TRANSFER, "%s: failed to generate file name", __func__);
        free(context.memory);
        return TRANSFER_FAIL_LOCAL;
    }
    List_ForEach(dispatcher->dispatcher->addressList, HTTP_Store, &context);
    free(context.memory);
    free(context.filename);
    return context.status;
}

static const char * tempFileTemplate = "rzb-XXXXXX";
struct FetchContext {
    struct Block *block;
    char * filename;
    char * tmpFileName;
    FILE * fd;
    uint8_t protocol;
    uint16_t port;
    enum TransferStatus status;
    size_t size;
    size_t expectedSize;
};

static bool
HTTP_OpenFetchFile(struct FetchContext *context)
{
    const char *tempDir;
    int fd;

    ASSERT(context != NULL);
    if (context == NULL) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: fetch context is NULL", __func__);
        return false;
    }

    tempDir = HTTP_GetTempDirectory();
    if (asprintf(&context->tmpFileName, "%s/%s", tempDir, tempFileTemplate) == -1) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: failed to allocate temp file template",
                __func__);
        context->tmpFileName = NULL;
        return false;
    }

    if ((fd = mkstemp(context->tmpFileName)) == -1) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: failed to create temp file: %s",
                __func__, strerror(errno));
        free(context->tmpFileName);
        context->tmpFileName = NULL;
        return false;
    }

    if ((context->fd = fdopen(fd, "w+b")) == NULL) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: failed to open temp file stream: %s",
                __func__, strerror(errno));
        close(fd);
        remove(context->tmpFileName);
        free(context->tmpFileName);
        context->tmpFileName = NULL;
        return false;
    }

    return true;
}


static size_t
WriteFileCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    struct FetchContext *req = (struct FetchContext *)userp;
    size_t written;


    written = fwrite(contents, size, nmemb, req->fd);


    req->size += written;

    return written;
}

static int
HTTP_Try_Fetch(void *i, void*ud)
{
    struct FetchContext *status = ud;
    char *address = i;
    char *url = NULL;
    CURL *curl = NULL;
    struct curl_slist *requestHeaders = NULL;
    TelemetrySpan_t *requestSpan = NULL;
    long http_code = 0;
    CURLcode res;
    const char *requestError = NULL;
    bool spanSuccess = false;


    rewind(status->fd);
    if (ftruncate(fileno(status->fd), 0) != 0) {
        rzb_log(LOG_ERR, LOG_C_TRANSFER, "%s: Failed to truncate fetch file", __func__);
        status->status = TRANSFER_FAIL_LOCAL;
        return LIST_EACH_OK;
    }
    status->size = 0;
    if (!HTTP_BuildURL(&url, status->protocol, address, status->port, status->filename,
                       __func__)) {
        status->status = TRANSFER_FAIL_LOCAL;
        return LIST_EACH_OK;
    }
    requestSpan = Telemetry_StartSpanWithKind("GET",
                                              NULL,
                                              TELEMETRY_SPAN_KIND_CLIENT);
    HTTP_AddCommonTelemetryAttributes(requestSpan,
                                      status->block,
                                      status->protocol,
                                      address,
                                      status->port,
                                      url,
                                      "GET");
    Telemetry_AddIntAttribute(requestSpan, "rzb.transfer.bytes_expected",
                              (int64_t)status->expectedSize);
    rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Attempting to fetch %s from %s", __func__, status->filename, url);
    curl = curl_easy_init();
    if (curl == NULL) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to initialize curl", __func__);
        HTTP_AddErrorTypeAttribute(requestSpan, "curl.init");
        requestError = "failed to initialize curl";
        status->status = TRANSFER_FAIL_LOCAL;
        goto cleanup;
    }
    if (!HTTP_ApplyTelemetryHeaders(curl, &requestHeaders)) {
        rzb_log(LOG_WARNING, LOG_C_TRANSFER,
                "%s: Failed to apply telemetry HTTP headers", __func__);
    }
    if (!HTTP_ConfigureCurl(curl, status->protocol, url) ||
            (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback) != CURLE_OK) ||
            (curl_easy_setopt(curl, CURLOPT_WRITEDATA, status) != CURLE_OK)
        )
    {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to configure HTTP transport request",
                __func__);
        HTTP_AddErrorTypeAttribute(requestSpan, "curl.request_config");
        requestError = "failed to configure HTTP transport request";
        status->status = TRANSFER_FAIL_LOCAL;
        goto cleanup;
    }

    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: curl_easy_perform() failed: %s", __func__,
                curl_easy_strerror(res));
        HTTP_AddErrorTypeAttribute(requestSpan, "curl.perform");
        requestError = curl_easy_strerror(res);
        status->status = TRANSFER_FAIL_DISPATCHER;
    }
    if (curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code) != CURLE_OK) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to get response code", __func__);
        HTTP_AddErrorTypeAttribute(requestSpan, "curl.response_code");
        requestError = "failed to get HTTP response code";
        status->status = TRANSFER_FAIL_LOCAL;
    }
cleanup:
    if (http_code > 0)
        Telemetry_AddIntAttribute(requestSpan, "http.response.status_code",
                                  (int64_t)http_code);
    Telemetry_AddIntAttribute(requestSpan, "rzb.transfer.bytes_transferred",
                              (int64_t)status->size);
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
    HTTP_FreeRequestHeaders(&requestHeaders);
    free(url);
    if (status->status == TRANSFER_FAIL_LOCAL) {
        if (requestError == NULL)
            requestError = "HTTP fetch request failed locally";
        Telemetry_EndSpan(requestSpan, false, requestError);
        return LIST_EACH_OK;
    }
    if (http_code != 200) {
        status->status = TRANSFER_FAIL_DISPATCHER;
        spanSuccess = (http_code >= 100 && http_code < 400);
        if (!spanSuccess)
            HTTP_AddStatusCodeErrorType(requestSpan, http_code);
        Telemetry_EndSpan(requestSpan, spanSuccess, spanSuccess ? NULL : requestError);
        return LIST_EACH_OK;
    }
    if (status->size != status->expectedSize) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: File size mismatch, got %zu expected %zu", __func__, status->size, status->expectedSize);
        status->status = TRANSFER_FAIL_DISPATCHER;
        HTTP_AddErrorTypeAttribute(requestSpan, "http.response.body.size_mismatch");
        requestError = "fetched file size did not match expected size";
        Telemetry_EndSpan(requestSpan, false, requestError);
        return LIST_EACH_OK;
    }
    status->status = TRANSFER_OK;
    Telemetry_EndSpan(requestSpan, true, NULL);
    return LIST_EACH_END;
}

static int
HTTP_Fetch(void *i, void*ud) {
    int try = 0;
    int max_tries = 10;
    struct FetchContext *status = ud;
    int res;
    for (try = 0; try < max_tries; try++) {
        res = HTTP_Try_Fetch(i, ud);
        if (status->status == TRANSFER_OK) {
            return res;
        }
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to fetch file, retrying %d/%d", __func__, try+1, max_tries);
    }
    return res;
}

SO_PUBLIC enum TransferStatus
Transfer_HTTP_Fetch(struct Block *block, struct ConnectedEntity *dispatcher)
{
    struct FetchContext context = {
        .block = block,
        .filename = NULL,
        .tmpFileName = NULL,
        .fd = NULL,
        .protocol = dispatcher->dispatcher->protocol,
        .port = dispatcher->dispatcher->port,
        .status = TRANSFER_FAIL_LOCAL,
        .size = 0,
        .expectedSize = block->pId->iLength,
    };
    if ((context.filename = Transfer_generateFilename (block)) == NULL)
    {
        rzb_log (LOG_ERR,LOG_C_TRANSFER, "%s: failed to generate file name", __func__);
        return TRANSFER_FAIL_LOCAL;
    }
    if (!HTTP_OpenFetchFile(&context)) {
        free(context.filename);
        return TRANSFER_FAIL_LOCAL;
    }
    rzb_log(LOG_DEBUG,LOG_C_TRANSFER, "%s: Storing file in: %s", __func__ , context.tmpFileName);
    List_ForEach(dispatcher->dispatcher->addressList, HTTP_Fetch, &context);
    fclose(context.fd);
    free(context.filename);
    if (context.status != TRANSFER_OK) {
        rzb_log(LOG_ERR,LOG_C_TRANSFER, "%s: Failed to fetch file", __func__);
        remove(context.tmpFileName);
        free(context.tmpFileName);
        return context.status;
    }
    return Transfer_Prepare_File(block, context.tmpFileName, true) ? TRANSFER_OK : TRANSFER_FAIL_LOCAL;
}
