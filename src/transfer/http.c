#include "config.h"
#include <razorback/debug.h>
#include <razorback/types.h>
#include <razorback/list.h>

#include <razorback/log.h>
#include <razorback/hash.h>
#include <razorback/block_pool.h>
#include <razorback/transfer.h>

#include <curl/curl.h>
#include <string.h>

#include "transfer/core.h"

static struct TransportDescriptor descriptor =
 {
         2,
         "Http",
         "Transfer file via shared file system",
         Transfer_HTTP_Store,
         Transfer_HTTP_Fetch
 };

bool HTTP_Init(void)
{
    return Transport_Register(&descriptor);
}

struct StoreContext
{
    struct BlockPoolItem *item;
    struct BlockPoolData *dataItem;
    size_t bytesRead;
    char * filename;
    uint16_t port;
    enum TransferStatus status;
    char * memory;
    size_t size;
};

static size_t
read_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    struct StoreContext *context = (struct StoreContext *)userdata;
    //rzb_log(LOG_DEBUG, "%s: Reading %zu blocks of size %zu", __func__, nitems, size);
    if (context->dataItem == NULL) {
       // rzb_log(LOG_DEBUG, "%s: No more data to read", __func__);
        return 0;
    }
    if (context->dataItem->iFlags == BLOCK_POOL_DATA_FLAG_FILE) {
        // If the data is a file, read from the file handle
        if (context->bytesRead + size * nitems > context->dataItem->iLength) {
            // If we are trying to read more data than is available in the file
            nitems = (context->dataItem->iLength - context->bytesRead);
            //rzb_log(LOG_DEBUG, "%s: Read too much data from file, read truncated: %zu", __func__, nitems);
        }
        context->bytesRead+= size * nitems;
        return fread(buffer,size, nitems, context->dataItem->data.file);
    } else {

        size_t want = size * nitems;
        size_t read = 0;
        while (read < want ) {
            //rzb_log(LOG_DEBUG, "%s: Want %zu bytes, read %zu bytes", __func__, want, read);
            // If we have run out of buffers just return the requested size as to not read random data
            if (context->dataItem == NULL) {
              //  rzb_log(LOG_DEBUG, "%s: No more data to read", __func__);
                return read;
            }
            // How much data is left to read in the current buffer
            //rzb_log(LOG_DEBUG, "%s: Current buffer length %zu, bytes read %zu", __func__, context->dataItem->iLength, context->bytesRead);
            size_t avail = context->dataItem->iLength - context->bytesRead;
            size_t remain = want - read;
            // If more data is wanted that in the current buffer then only
            // copy the available data in the current buffer
            size_t to_read = avail < want ? avail : want;
            to_read = remain < to_read ? remain : to_read;
            //rzb_log(LOG_DEBUG, "%s: Reading %zu bytes from buffer - avail %zu - want %zu", __func__, to_read, avail, want);
            memcpy(buffer+read, context->dataItem->data.pointer + context->bytesRead, to_read);
            read += to_read;
            context->bytesRead += to_read;
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
        rzb_log(LOG_ERR,"%s: not enough memory (realloc returned NULL)", __func__ );
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
    long http_code = 0;

    // Reset all the context states incase this is a retry
    status->dataItem = status->item->pDataHead;
    status->bytesRead = 0;
    if (status->dataItem->iFlags == BLOCK_POOL_DATA_FLAG_FILE) {
        rewind(status->dataItem->data.file);
    }

    if (asprintf(&url, "http://%s:%d/%c/%c/%c/%c/%s",
                address,
                status->port,
                 status->filename[0],
                 status->filename[1],
                 status->filename[2],
                 status->filename[3],
                 status->filename) == -1)
    {
        rzb_log(LOG_ERR, "%s: Failed to generate URL", __func__);
        status->status = TRANSFER_FAIL_LOCAL;
        return LIST_EACH_OK;
    }
    rzb_log(LOG_DEBUG, "%s: Attempting to store %s at %s", __func__, status->filename, url);
    CURL *curl = curl_easy_init();
    if (curl == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to initialize curl", __func__);
        free(url);
        status->status = TRANSFER_FAIL_LOCAL;
        return LIST_EACH_OK;
    }
    curl_mime *mime = curl_mime_init(curl);
    if (mime == NULL) {

    }
    curl_mimepart *part = curl_mime_addpart(mime);
    if (part == NULL) {
    }

    if (
            (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) ||
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
        rzb_log(LOG_ERR, "%s: Failed to set URL", __func__);
        curl_easy_cleanup(curl);
        free(url);
        status->status = TRANSFER_FAIL_LOCAL;
        return LIST_EACH_OK;
    }

    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        rzb_log(LOG_ERR, "%s: curl_easy_perform() failed: %s", __func__,
                curl_easy_strerror(res));
        status->status = TRANSFER_FAIL_DISPATCHER;
    }
    if (curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code) != CURLE_OK) {
        rzb_log(LOG_ERR, "%s: Failed to get response code", __func__);
        status->status = TRANSFER_FAIL_LOCAL;
    }

curl_easy_cleanup(curl);
    curl_mime_free(mime);

    free(url);
    // Rewind the filehandle after the request
    if (status->dataItem != NULL && status->dataItem->iFlags == BLOCK_POOL_DATA_FLAG_FILE)
    {
        rewind(status->dataItem->data.file);
    }
    if (http_code != 200) {
        rzb_log(LOG_ERR, "%s: Failed to store file: %zi", __func__, http_code);
        rzb_log(LOG_ERR, "%s: Failed to store file: %s", __func__, status->memory);
        status->status = TRANSFER_FAIL_DISPATCHER;
        return LIST_EACH_OK;
    }

    rzb_log(LOG_DEBUG, "%s: Successfully stored file", __func__);
    status->status = TRANSFER_OK;
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
        rzb_log(LOG_ERR, "%s: Failed to store file, retrying %d/%d", __func__, try+1, max_tries);
    }
    return res;
}

SO_PUBLIC enum TransferStatus
Transfer_HTTP_Store(struct BlockPoolItem *item, struct ConnectedEntity *dispatcher)
{
    struct StoreContext context = {
        .item = item,
        .dataItem = item->pDataHead,
        .bytesRead = 0,
        .filename = NULL,
        .port = dispatcher->dispatcher->port,
        .status = TRANSFER_FAIL_LOCAL,
        .memory = malloc(1),
        .size = 0,
    };
    ASSERT(item != NULL);
    ASSERT(dispatcher != NULL);
    if ((context.filename = Transfer_generateFilename (item->pEvent->pBlock)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: failed to generate file name", __func__);
        free(context.memory);
        return TRANSFER_FAIL_LOCAL;
    }
    List_ForEach(dispatcher->dispatcher->addressList, HTTP_Store, &context);
    free(context.memory);
    free(context.filename);
    return context.status;
}

static const char * tempFileTemplate = "/tmp/rzb-XXXXXX";
struct FetchContext {
    char * filename;
    char * tmpFileName;
    FILE * fd;
    uint16_t port;
    enum TransferStatus status;
    size_t size;
};


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
    long http_code = 0;
    CURLcode res;


    rewind(status->fd);
    ftruncate(fileno(status->fd), 0);
    status->size = 0;
    if (asprintf(&url, "http://%s:%d/%c/%c/%c/%c/%s",
                 address,
                 status->port,
                 status->filename[0],
                 status->filename[1],
                 status->filename[2],
                 status->filename[3],
                 status->filename) == -1) {
        rzb_log(LOG_ERR, "%s: Failed to generate URL", __func__);
        status->status = TRANSFER_FAIL_LOCAL;
        return LIST_EACH_OK;
    }
    rzb_log(LOG_DEBUG, "%s: Attempting to store %s at %s", __func__, status->filename, url);
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        rzb_log(LOG_ERR, "%s: Failed to initialize curl", __func__);
        free(url);
        status->status = TRANSFER_FAIL_LOCAL;
        return LIST_EACH_OK;
    }
    if (
            (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) ||
            (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback) != CURLE_OK) ||
            (curl_easy_setopt(curl, CURLOPT_WRITEDATA, status) != CURLE_OK)
        )
    {
        rzb_log(LOG_ERR, "%s: Failed to set URL", __func__);
        curl_easy_cleanup(curl);
        free(url);
        status->status = TRANSFER_FAIL_LOCAL;
        return LIST_EACH_OK;
    }

    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK) {
        rzb_log(LOG_ERR, "%s: curl_easy_perform() failed: %s", __func__,
                curl_easy_strerror(res));
        status->status = TRANSFER_FAIL_DISPATCHER;
    }
    curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    free(url);
    if (http_code != 200) {
        status->status = TRANSFER_FAIL_DISPATCHER;
        return LIST_EACH_OK;
    }
    status->status = TRANSFER_OK;
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
        rzb_log(LOG_ERR, "%s: Failed to fetch file, retrying %d/%d", __func__, try+1, max_tries);
    }
    return res;
}

SO_PUBLIC enum TransferStatus
Transfer_HTTP_Fetch(struct Block *block, struct ConnectedEntity *dispatcher)
{
    struct FetchContext context = {
        .filename = NULL,
        .tmpFileName = NULL,
        .fd = NULL,
        .port = dispatcher->dispatcher->port,
        .status = TRANSFER_FAIL_LOCAL,
        .size = 0,
    };
    if ((context.filename = Transfer_generateFilename (block)) == NULL)
    {
        rzb_log (LOG_ERR, "%s: failed to generate file name", __func__);
        return TRANSFER_FAIL_LOCAL;
    }
    context.tmpFileName = calloc(strlen(tempFileTemplate)+1, sizeof(char));
    strcpy(context.tmpFileName, tempFileTemplate);
    int f = mkstemp(context.tmpFileName );
    context.fd = fdopen(f, "w");
    rzb_log(LOG_DEBUG, "%s: Storing file in: %s", __func__ , context.tmpFileName);
    List_ForEach(dispatcher->dispatcher->addressList, HTTP_Fetch, &context);
    fclose(context.fd);
    free(context.filename);
    if (context.status != TRANSFER_OK) {
        rzb_log(LOG_ERR, "%s: Failed to fetch file", __func__);
        free(context.tmpFileName);
        return context.status;
    }
    return Transfer_Prepare_File(block, context.tmpFileName, true) ? TRANSFER_OK : TRANSFER_FAIL_LOCAL;
}
