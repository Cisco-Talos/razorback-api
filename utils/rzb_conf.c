#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libconfig.h>

#include "rzb_conf.h"

#define API_CONFIG_FILE "rzb.conf"

SO_PUBLIC RZBConfig rzbconfig;

// Libconfig handle
config_t config;

typedef enum
{
    CONFIGURED, UNCONFIGURED
} CONFIGSTATE;

struct RZBConfigFile {
    RZBConfKey_t *type;
    config_t config;
    struct RZBConfigFile *next;
};

struct RZBConfigFile;
typedef struct RZBConfigFile RZBConfigFile;

RZBConfigFile *configList = NULL;

CONFIGSTATE configState = UNCONFIGURED;

HRESULT testFile(const char *);
HRESULT parseBlock(config_t *, RZBConfKey_t *);
char* getConfigFile(const char *, const char *);

static RZBConfKey_t global_config[] = {
    { "GLOBAL.MAXTHREADS", RZB_CONF_KEY_TYPE_UNSIGNED, &rzbconfig.maximumthreads },
    { "GLOBAL.BADMD5CACHESIZE", RZB_CONF_KEY_TYPE_UNSIGNED, &rzbconfig.badcachesize },
    { "GLOBAL.GOODMD5CACHESIZE", RZB_CONF_KEY_TYPE_UNSIGNED, &rzbconfig.goodcachesize },
    { "GLOBAL.URLCACHESIZE", RZB_CONF_KEY_TYPE_UNSIGNED, &rzbconfig.urlcachesize },
    { NULL, RZB_CONF_KEY_TYPE_END, NULL }
};

static RZBConfKey_t nugget_server_config[] = {
    { "NUGGETSRV.NUGNAME", RZB_CONF_KEY_TYPE_STRING, &rzbconfig.nugname },
    { "NUGGETSRV.NUGPORT", RZB_CONF_KEY_TYPE_STRING, &rzbconfig.nugport },
    { "NUGGETSRV.NUGADDR", RZB_CONF_KEY_TYPE_STRING, &rzbconfig.nugaddr },
    { "NUGGETSRV.HANDLERDIR", RZB_CONF_KEY_TYPE_STRING, &rzbconfig.handlerdir },
    { NULL, RZB_CONF_KEY_TYPE_END, NULL }
};

static RZBConfKey_t dispatcher_config[] = {
    { "DISPATCHSRV.IP", RZB_CONF_KEY_TYPE_STRING, &rzbconfig.dsrvaddr },
    { "DISPATCHSRV.PORT", RZB_CONF_KEY_TYPE_STRING, &rzbconfig.dsrvport },
    { "DISPATCHSRV.ROUTINGTYPE", RZB_CONF_KEY_TYPE_ROUTING_TYPE, &rzbconfig.routingtype },
    { NULL, RZB_CONF_KEY_TYPE_END, NULL }
};

static RZBConfKey_t testing_config[] = {
    { "TESTING.DIR", RZB_CONF_KEY_TYPE_STRING, &rzbconfig.testing_file_dir },
    { NULL, RZB_CONF_KEY_TYPE_END, NULL }
};

char *getConfigFile(const char *configDir, const char *configFile) {
    if (configDir == NULL) {
        configDir = ETC_DIR;
    }
    char *result;
    result = malloc(strlen(configDir) + strlen(configFile) +2 );
    if (result == NULL) 
        return NULL;

    strncpy(result, configDir, strlen(configDir)+1);
    strncat(result, "/", 1);
    strncat(result, configFile, strlen(configFile));

    return result;
}

SO_PUBLIC HRESULT readApiConfig(const char *configDir) {
    if (configState == CONFIGURED) {
        printf("WARNING: readConfig called twice\n");
        return R_FAIL;
    }
    int config_status;

    memset(&config, 0, sizeof(config));
    memset(&rzbconfig, 0, sizeof(rzbconfig));
    rzbconfig.network_to_secs = 10;

    config_init(&config);
    char * configfile = getConfigFile(configDir, API_CONFIG_FILE);
    if (testFile(configfile) != R_SUCCESS) {
        free(configfile);
        return R_FAIL;
    }
    config_status = config_read_file(&config,configfile);

    if (config_status != CONFIG_TRUE)
        printf("%s\n", config_error_text(&config));

    parseBlock(&config,  global_config);
    parseBlock(&config,  nugget_server_config);
    parseBlock(&config,  dispatcher_config);
    parseBlock(&config,  testing_config);
    configState=CONFIGURED;
    return R_SUCCESS;
}

SO_PUBLIC HRESULT readMyConfig(const char *configDir, const char *configFile, RZBConfKey_t *config_fmt) {
    if (configFile == NULL) {
        printf("FATAL: readMyConfigFile configFile was null\n");
        return R_FAIL;
    }
    char * configfile = getConfigFile(configDir, configFile);
    RZBConfigFile file;
    memset(&file, 0, sizeof(RZBConfigFile));
    file.type = config_fmt;
    if (testFile(configfile) != R_SUCCESS) {
        free(configfile);
        return R_FAIL;
    }
    if (config_read_file(&file.config,configfile) != CONFIG_TRUE)
        printf("%s\n", config_error_text(&config));

    if (configList == NULL) {
        configList = &file;
    } else {
        file.next = configList;
        configList = &file;
    }
    return parseBlock(&file.config, config_fmt);
}

HRESULT parseBlock(config_t *config, RZBConfKey_t *block) { 
    int status = CONFIG_TRUE;
#if (((LIBCONFIG_VER_MAJOR == 1) && (LIBCONFIG_VER_MINOR >= 4)) \
       || (LIBCONFIG_VER_MAJOR > 1))
    /* use features present in libconfig 1.4 and later */
    int t;
#else
    long t;
#endif
    config_setting_t *tt;
    while (block->type != RZB_CONF_KEY_TYPE_END) {
        tt = config_lookup(config, block->key);
        if ( tt == NULL) {
            printf("WARNING: Cant find key: %s\n", block->key);
            block++;
            continue;
        }

        if (block->type == RZB_CONF_KEY_TYPE_UNSIGNED) { 
            status = config_lookup_int(config, block->key, &t);
            *(unsigned *)(block->dest)= (unsigned)t;
        } else if (block->type == RZB_CONF_KEY_TYPE_SIGNED) {
            status = config_lookup_int(config, block->key, &t);
            *(int *)(block->dest)= (int)t;
        } else if (block->type == RZB_CONF_KEY_TYPE_STRING) {
            status = config_lookup_string(config, block->key, block->dest);
        } else if (block->type == RZB_CONF_KEY_TYPE_ROUTING_TYPE) {
            const char *type;
            status = config_lookup_string(config, block->key, &type);
            if (status != CONFIG_TRUE) {
                printf("%s\n", config_error_text(config));
                return R_FAIL;
            }
            t = 2;
            if (!strncasecmp(type, "opaque", 6))
                t=0;
            else if (!strncasecmp(type, "transparent", 11))
                t=1;
            *(int *)(block->dest)= (int)t;

        } else {
            printf("Unknown config attribute type.");
            return R_FAIL;
        }
        if (status != CONFIG_TRUE) {
            printf("%s\n", config_error_text(config));
            return R_FAIL;
        }
        block++;
    }
    return R_SUCCESS;
}

HRESULT testFile(const char * configfile) {
    struct stat sb;
    int fd = open(configfile, O_RDONLY);

    if (fd == -1)
    {
        fprintf(stderr, "Failed to open (%s) in ", configfile);
        perror("readConfig - open");
        return R_FAIL;
    }

    if (fstat(fd, &sb) == -1)
    {
        perror("fstat");
        return R_FAIL;
    }
    close(fd);
    return R_SUCCESS;

}

SO_PUBLIC void rzbConfCleanUp(void)
{
    config_destroy(&config);
    while (configList != NULL) {
        config_destroy(&configList->config);
        configList = configList->next;
    }
}

