#include "config.h"
#include <razorback/config_file.h>
#include <razorback/hash.h>
#include "runtime_config.h"
#include "init.h"

#include <string.h>
#define API_CONFIG_FILE "api.conf"

bool parseHashType (const char *, conf_int_t *);

// Maximum Number of Threads
static conf_int_t sg_iThreadLimit;

// Configured HASH Type.
static conf_int_t sg_iHashType;

// Maximum Block Submission Size
static conf_int_t sg_iMaxBlockSize;
static conf_int_t sg_iHelloTime;
static conf_int_t sg_iDeadTime;

static conf_int_t sg_iLocality;

// Message Queue Stuff
static char *sg_sMqHost;
static conf_int_t sg_iMqPort;
static char *sg_sMqUser;
static char *sg_sMqPassword;
static bool sg_sMqSSL;

// Local Cache Stuff
static conf_int_t sg_iCacheGoodLimit;
static conf_int_t sg_iCacheBadLimit;

// Logging Stuff
static RZB_LOG_DEST_t log_dest = RZB_LOG_DEST_ERR;
static int log_level = LOG_INFO;
static int log_facility;
static char *log_file;

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
SO_PUBLIC bool
Config_getMqSSL (void)
{
    return sg_sMqSSL;
}

SO_PUBLIC uint32_t
Config_getCacheGoodLimit (void)
{
    return sg_iCacheGoodLimit;
}

SO_PUBLIC uint32_t
Config_getLocality (void)
{
    return sg_iLocality;
}

SO_PUBLIC uint32_t
Config_getCacheBadLimit (void)
{
    return sg_iCacheBadLimit;
}

void 
Config_setLogDest (RZB_LOG_DEST_t dest)
{
    log_dest = dest;
}
void 
Config_setLogLevel(int level)
{
    log_level = level;
}

RZB_LOG_DEST_t 
Config_getLogDest (void)
{
    return log_dest;
}

int 
Config_getLogLevel(void)
{
    return log_level;
}

int 
Config_getLogFacility(void)
{
    return log_facility;
}

char * 
Config_getLogFile(void)
{
    return log_file;
}

static RZBConfCallBack hashCallback = {
    &parseHashType
};

static bool parseLogDest (const char *, conf_int_t *);
static bool parseLogLevel (const char *, conf_int_t *);
static bool parseLogFacility (const char *, conf_int_t *);

static RZBConfCallBack destCallBack = {
    &parseLogDest
};

static RZBConfCallBack levelCallBack = {
    &parseLogLevel
};

static RZBConfCallBack facilityCallBack = {
    &parseLogFacility
};



static RZBConfKey_t global_config[] = {
    // Global Items
    {"Global.MaxThreads", RZB_CONF_KEY_TYPE_INT, &sg_iThreadLimit, NULL},
    {"Global.MaxBlockSize", RZB_CONF_KEY_TYPE_INT, &sg_iMaxBlockSize, NULL},
    {"Global.HashType", RZB_CONF_KEY_TYPE_PARSED_STRING,
     &sg_iHashType, &hashCallback},
    {"Global.HelloTime", RZB_CONF_KEY_TYPE_INT, &sg_iHelloTime, NULL},
    {"Global.DeadTime", RZB_CONF_KEY_TYPE_INT, &sg_iDeadTime, NULL},
    {"Global.Locality", RZB_CONF_KEY_TYPE_INT, &sg_iLocality, NULL},

    // Local Cache Items
    {"Cache.GoodLimit", RZB_CONF_KEY_TYPE_INT, &sg_iCacheGoodLimit, NULL},
    {"Cache.BadLimit", RZB_CONF_KEY_TYPE_INT, &sg_iCacheBadLimit, NULL},

    // Message Queue Stuff
    {"MessageQueue.Host", RZB_CONF_KEY_TYPE_STRING, &sg_sMqHost, NULL},
    {"MessageQueue.Port", RZB_CONF_KEY_TYPE_INT, &sg_iMqPort, NULL},
    {"MessageQueue.User", RZB_CONF_KEY_TYPE_STRING, &sg_sMqUser, NULL},
    {"MessageQueue.Password", RZB_CONF_KEY_TYPE_STRING, &sg_sMqPassword, NULL},
    {"MessageQueue.SSL", RZB_CONF_KEY_TYPE_BOOL, &sg_sMqSSL, NULL},

    // Logging Stuff
    {"Log.Destination", RZB_CONF_KEY_TYPE_PARSED_STRING, &log_dest, &destCallBack},
    {"Log.Level", RZB_CONF_KEY_TYPE_PARSED_STRING, &log_level,
     &levelCallBack},
    {"Log.Syslog_Facility", RZB_CONF_KEY_TYPE_PARSED_STRING, &log_facility,
     &facilityCallBack},
    {"Log.File", RZB_CONF_KEY_TYPE_STRING, &log_file, NULL},
    {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
};

void readApiConfig (void)
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


static bool
parseLogDest (const char *string, conf_int_t * val)
{
    if (!strncasecmp (string, "syslog", 6))
    {
        *val = RZB_LOG_DEST_SYSLOG;
        return true;
    }
    else if (!strncasecmp (string, "stderr", 6))
    {
        *val = RZB_LOG_DEST_ERR;
        return true;
    }
    else if (!strncasecmp (string, "file", 4))
    {
        *val = RZB_LOG_DEST_FILE;
        return true;
    }
    return false;
}

static bool
parseLogLevel (const char *string, conf_int_t * val)
{
    if (!strncasecmp (string, "emergency", 9))
    {
        *val = LOG_EMERG;
        return true;
    }
    else if (!strncasecmp (string, "alert", 5))
    {
        *val = LOG_ALERT;
        return true;
    }
    else if (!strncasecmp (string, "critical", 8))
    {
        *val = LOG_CRIT;
        return true;
    }
    else if (!strncasecmp (string, "error", 5))
    {
        *val = LOG_ERR;
        return true;
    }
    else if (!strncasecmp (string, "warning", 7))
    {
        *val = LOG_WARNING;
        return true;
    }
    else if (!strncasecmp (string, "notice", 6))
    {
        *val = LOG_NOTICE;
        return true;
    }
    else if (!strncasecmp (string, "info", 4))
    {
        *val = LOG_INFO;
        return true;
    }
    else if (!strncasecmp (string, "debug", 5))
    {
        *val = LOG_DEBUG;
        return true;
    }
    return false;
}

static bool
parseLogFacility (const char *string, conf_int_t * val)
{
    if (!strncasecmp (string, "daemon", 6))
    {
        *val = LOG_DAEMON;
        return true;
    }
    else if (!strncasecmp (string, "user", 4))
    {
        *val = LOG_USER;
        return true;
    }
    else if (!strncasecmp (string, "local0", 6))
    {
        *val = LOG_LOCAL0;
        return true;
    }
    else if (!strncasecmp (string, "local1", 6))
    {
        *val = LOG_LOCAL1;
        return true;
    }
    else if (!strncasecmp (string, "local2", 6))
    {
        *val = LOG_LOCAL2;
        return true;
    }
    else if (!strncasecmp (string, "local3", 6))
    {
        *val = LOG_LOCAL3;
        return true;
    }
    else if (!strncasecmp (string, "local4", 6))
    {
        *val = LOG_LOCAL4;
        return true;
    }
    else if (!strncasecmp (string, "local5", 6))
    {
        *val = LOG_LOCAL5;
        return true;
    }
    else if (!strncasecmp (string, "local6", 6))
    {
        *val = LOG_LOCAL6;
        return true;
    }
    else if (!strncasecmp (string, "local7", 6))
    {
        *val = LOG_LOCAL7;
        return true;
    }
    return false;
}
