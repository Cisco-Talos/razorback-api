#include "config.h"

#include <uuid/uuid.h>
#include <stdlib.h>
#include <string.h>
#include <magic.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <openssl/evp.h>

#include "rzb_alert_util.h"
#include "rzb_global.h"

static int checkMagic(const void *data, size_t len, char *result);

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
        printf("Error allocating space for md5\n");

    return md5_string;
}

const unsigned char *file_type_lookup(const void *data, size_t len)
{
    char file_info[MAX_DESCRIPTION_SIZE];
    const unsigned char *filetype = NO_DATA_TYPE;

    if ((checkMagic(data, len, file_info)) == 0)
    {
        if (strstr(file_info, "PDF") != NULL)
            filetype = PDF_FILE;
        else if (strstr(file_info, "PE32") != NULL)
            filetype = PE_FILE;
        else if (strstr(file_info, "Flash") != NULL)
            filetype = SWF_FILE;
        else if (strstr(file_info, "Zip archive") != NULL)
            filetype = ZIP_FILE;
    }
    else
    {
        perror("Error looking up file information");
    }

    return filetype;
}

static int checkMagic(const void *data, size_t len, char *result)
{
    const char *magic_full;
    magic_t magic_cookie;

    if ((magic_cookie = magic_open(MAGIC_NONE)) == NULL)
    {
        perror("Error creating magic_cookie");
        return -1;
    }

    if (magic_load(magic_cookie, NULL) != 0)
    {
        magic_close(magic_cookie);
        return -1;
    }

    if ((magic_full = magic_buffer(magic_cookie, data, len)) == NULL)
    {
        perror("Error reading file type");
        return -1;
    }

    snprintf(result, MAX_DESCRIPTION_SIZE, "%s", magic_full);
    magic_close(magic_cookie);

    return 0;
}
