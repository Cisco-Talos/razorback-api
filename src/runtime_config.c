#include "config.h"
#include <razorback/config_file.h>
#include <razorback/hash.h>
#include "runtime_config.h"

#include <string.h>
#define API_CONFIG_FILE "rzb.conf"

bool parseHashType (const char *, conf_int_t *);

// Maximum Number of Threads
static conf_int_t sg_iThreadLimit;

// Configured HASH Type.
static conf_int_t sg_iHashType;

// Maximum Block Submission Size
static conf_int_t sg_iMaxBlockSize;
static conf_int_t sg_iHelloTime;
static conf_int_t sg_iDeadTime;

// Message Queue Stuff
static char *sg_sMqHost;
static conf_int_t sg_iMqPort;
static char *sg_sMqUser;
static char *sg_sMqPassword;

// Local Cache Stuff
static conf_int_t sg_iCacheGoodLimit;
static conf_int_t sg_iCacheBadLimit;

SO_PUBLIC uint32_t
Config_getHashType (void)
{
    return sg_iHashType;
}

SO_PUBLIC uint32_t
Config_getMaxBlockSize (void)
{
    return sg_iMaxBlockSize;
}

SO_PUBLIC uint32_t
Config_getThreadLimit (void)
{
    return sg_iThreadLimit;
}

SO_PUBLIC uint32_t
Config_getHelloTime (void)
{
    return sg_iHelloTime;
}

SO_PUBLIC uint32_t
Config_getDeadTime (void)
{
    return sg_iDeadTime;
}

SO_PUBLIC char *
Config_getMqHost (void)
{
    return sg_sMqHost;
}

SO_PUBLIC uint32_t
Config_getMqPort (void)
{
    return sg_iMqPort;
}

SO_PUBLIC char *
Config_getMqUser (void)
{
    return sg_sMqUser;
}

SO_PUBLIC char *
Config_getMqPassword (void)
{
    return sg_sMqPassword;
}

SO_PUBLIC uint32_t
Config_getCacheGoodLimit (void)
{
    return sg_iCacheGoodLimit;
}

SO_PUBLIC uint32_t
Config_getCacheBadLimit (void)
{
    return sg_iCacheBadLimit;
}

static RZBConfCallBack hashCallback = {
    &parseHashType
};

static RZBConfKey_t global_config[] = {
    // Global Items
    {"Global.MaxThreads", RZB_CONF_KEY_TYPE_INT, &sg_iThreadLimit, NULL},
    {"Global.MaxBlockSize", RZB_CONF_KEY_TYPE_INT, &sg_iMaxBlockSize, NULL},
    {"Global.HashType", RZB_CONF_KEY_TYPE_PARSED_STRING,
     &sg_iHashType, &hashCallback},
    {"Global.HelloTime", RZB_CONF_KEY_TYPE_INT, &sg_iHelloTime, NULL},
    {"Global.DeadTime", RZB_CONF_KEY_TYPE_INT, &sg_iDeadTime, NULL},

    // Local Cache Items
    {"Cache.GoodLimit", RZB_CONF_KEY_TYPE_INT, &sg_iCacheGoodLimit, NULL},
    {"Cache.BadLimit", RZB_CONF_KEY_TYPE_INT, &sg_iCacheBadLimit, NULL},

    // Message Queue Stuff
    {"MessageQueue.Host", RZB_CONF_KEY_TYPE_STRING, &sg_sMqHost, NULL},
    {"MessageQueue.Port", RZB_CONF_KEY_TYPE_INT, &sg_iMqPort, NULL},
    {"MessageQueue.User", RZB_CONF_KEY_TYPE_STRING, &sg_sMqUser, NULL},
    {"MessageQueue.Password", RZB_CONF_KEY_TYPE_STRING, &sg_sMqPassword,
     NULL},


    {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
};

SO_PUBLIC void __attribute__ ((constructor)) readApiConfig (void)
{
    readMyConfig (NULL, API_CONFIG_FILE, global_config);
    sg_iMaxBlockSize = sg_iMaxBlockSize * 1024 * 1024;  // Convert to MB;
}


bool
parseHashType (const char *string, conf_int_t * val)
{
    if (!strncasecmp (string, "MD5", 3))
    {
        *val = HASH_TYPE_MD5;
        return true;
    }
    else if (!strncasecmp (string, "SHA1", 4))
    {
        *val = HASH_TYPE_SHA1;
        return true;
    }
    else if (!strncasecmp (string, "SHA224", 6))
    {
        *val = HASH_TYPE_SHA224;
        return true;
    }
    else if (!strncasecmp (string, "SHA256", 6))
    {
        *val = HASH_TYPE_SHA256;
        return true;
    }
    else if (!strncasecmp (string, "SHA512", 6))
    {
        *val = HASH_TYPE_SHA512;
        return true;
    }
    return false;

}
