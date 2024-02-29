#include "config.h"

#include <uuid/uuid.h>
#include <stdlib.h>
#include <string.h>
#include <magic.h>
#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <openssl/evp.h>

#include "rzb_alert_util.h"
#include "rzb_log.h"
#include "rzb_global.h"

void md5sum(const void *content, ssize_t len, unsigned char *md5)
{
    EVP_MD_CTX mdctx;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned md_len;

    memset(md_value, 0, EVP_MAX_MD_SIZE);
    EVP_DigestInit(&mdctx, EVP_md5());
    EVP_DigestUpdate(&mdctx, content, (size_t) len);
    EVP_DigestFinal_ex(&mdctx, md_value, &md_len);
    EVP_MD_CTX_cleanup(&mdctx);
    memcpy(md5, md_value, RZB_HASH_SIZE);
}

char *md5sum_string(const void *content, ssize_t len)
{
    unsigned char md5[RZB_HASH_SIZE];
    char *md5_string;
    unsigned i;

    md5sum(content, len, md5);

    if ((md5_string = calloc(1, SAFE_MD5_SIZE)) != NULL)
    {
        for (i = 0; i < 16; i++)
            snprintf(&md5_string[i << 1], 3, "%02x", md5[i]);
    }
    else
        rzb_log(LOG_ERR, "Error allocating space for md5");

    return md5_string;
}

const unsigned char *file_type_lookup(const void *data, size_t len)
{
    const char *magic_full;
    magic_t magic_cookie;
    uuid_t found_type;

    if ((magic_cookie = magic_open(MAGIC_NO_CHECK_CDF)) == NULL)
    {
        rzb_log(LOG_ERR, "Error creating magic_cookie: %s", strerror(errno));
        return NO_DATA_TYPE;
    }

    if (magic_load(magic_cookie, MAGIC_FILE) != 0)
    {
        magic_close(magic_cookie);
        return NO_DATA_TYPE;
    }

    if ((magic_full = magic_buffer(magic_cookie, data, len)) == NULL)
    {
        rzb_log(LOG_ERR, "Error reading file type: %s", strerror(errno));
        return NO_DATA_TYPE;
    }

    if(strlen(magic_full) != 36) {
        rzb_log(LOG_ERR, "Unknown file type found: %s", magic_full);
        return NO_DATA_TYPE;
    }
    
    if(uuid_parse(magic_full, found_type) != 0) {
        rzb_log(LOG_ERR, "Unable to parse returned file type: %s", magic_full);
        return NO_DATA_TYPE;
    }

    if(!uuid_compare(found_type, PDF_FILE)) {
        return PDF_FILE;
    }
    else if(!uuid_compare(found_type, SWF_FILE)) {
        return SWF_FILE;
    }
    else if(!uuid_compare(found_type, ZIP_FILE)) {
        return ZIP_FILE;
    }
    else if(!uuid_compare(found_type, PE_FILE)) {
        return PE_FILE;
    }
    else if(!uuid_compare(found_type, OLE_DOC)) {
        return OLE_DOC;
    }
    else {
        return NO_DATA_TYPE;
    }
    magic_close(magic_cookie);
}
