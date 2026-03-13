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
#include <razorback/config_file.h>
#include <razorback/hash.h>
#include <razorback/messages.h>
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

static char *sg_sTransferPassword;
static char *sg_sTransferMode;

// Message Queue Stuff
static char *sg_sMqHost;
static conf_int_t sg_iMqPort;
static char *sg_sMqUser;
static char *sg_sMqPassword;
static char *sg_sMqVhost = NULL;
static bool sg_sMqSSL;
static conf_int_t sg_iMqPrefetch = 10;

// Local Cache Stuff
static conf_int_t sg_iCacheGoodLimit;
static conf_int_t sg_iCacheBadLimit;

// Logging Stuff
static RZB_LOG_DEST_t log_dest = RZB_LOG_DEST_ERR;
static int log_level = LOG_INFO;
static int log_facility;
static char *log_file;

// Locality Stuff
static conf_int_t sg_localityId =0;
static char *sg_localityBlockStore = NULL;
static bool sg_blockStoreRemote = false;
conf_int_t *sg_backupLocalities;
conf_int_t sg_backupLocalityCount = 0;


// Submission
conf_int_t sg_subGcReqThreadsInit =1;
conf_int_t sg_subGcReqThreadsMax =5;
conf_int_t sg_subGcRespThreadsInit =1;
conf_int_t sg_subGcRespThreadsMax =5;
conf_int_t sg_subTransferThreadsInit =1;
conf_int_t sg_subTransferThreadsMax =5;
conf_int_t sg_blockPoolMaxItems=0;
conf_int_t sg_blockPoolMaxSize=0;
conf_int_t sg_blockPoolMaxItemSize=0;

// Inspection
conf_int_t sg_inspThreadsInit =1;
conf_int_t sg_inspThreadsMax =5;

uint32_t
Config_getHashType (void)
{
    return sg_iHashType;
}

uint32_t
Config_getMaxBlockSize (void)
{
    return sg_iMaxBlockSize;
}

uint32_t
Config_getThreadLimit (void)
{
    return sg_iThreadLimit;
}

uint32_t
Config_getHelloTime (void)
{
    return sg_iHelloTime;
}

uint32_t
Config_getDeadTime (void)
{
    return sg_iDeadTime;
}

char *
Config_getMqHost (void)
{
    return sg_sMqHost;
}

uint32_t
Config_getMqPort (void)
{
    return sg_iMqPort;
}

char *
Config_getMqUser (void)
{
    return sg_sMqUser;
}

char *
Config_getMqPassword (void)
{
    return sg_sMqPassword;
}

char *
Config_getMqVhost (void)
{
    return sg_sMqVhost;
}

bool
Config_getMqSSL (void)
{
    return sg_sMqSSL;
}

uint32_t
Config_getMqPrefetch (void)
{
    return sg_iMqPrefetch;
}

uint32_t
Config_getCacheGoodLimit (void)
{
    return sg_iCacheGoodLimit;
}

uint32_t
Config_getLocalityId (void)
{
    return sg_localityId;
}
char *
Config_getLocalityBlockStore (void)
{
    return sg_localityBlockStore;
}
bool 
Config_isBlockStoreRemote(void)
{
    return sg_blockStoreRemote;
}
conf_int_t *
Config_getLocalityBackupOrder (void)
{
    return sg_backupLocalities;
}
conf_int_t
Config_getLocalityBackupCount (void)
{
    return sg_backupLocalityCount;
}

uint32_t
Config_getSubGcReqThreadsInit (void)
{
    return sg_subGcReqThreadsInit;
}
uint32_t
Config_getSubGcReqThreadsMax (void)
{
    return sg_subGcReqThreadsMax;
}

uint32_t
Config_getSubGcRespThreadsInit (void)
{
    return sg_subGcRespThreadsInit;
}
uint32_t
Config_getSubGcRespThreadsMax (void)
{
    return sg_subGcRespThreadsMax;
}

uint32_t
Config_getSubTransferThreadsInit (void)
{
    return sg_subTransferThreadsInit;
}

uint32_t
Config_getSubTransferThreadsMax (void)
{
    return sg_subTransferThreadsMax;
}

uint32_t
Config_getBlockPoolMaxItems (void)
{
    return sg_blockPoolMaxItems;
}

uint32_t
Config_getBlockPoolMaxSize (void)
{
    return sg_blockPoolMaxSize;
}

uint32_t
Config_getBlockPoolMaxItemSize (void)
{
    return sg_blockPoolMaxItemSize;
}

uint32_t
Config_getInspThreadsInit (void)
{
    return sg_inspThreadsInit;
}

uint32_t
Config_getInspThreadsMax (void)
{
    return sg_inspThreadsMax;
}

uint32_t
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

char *
Config_getTransferMode(void)
{
    return sg_sTransferMode;
}

SO_PUBLIC char *
Razorback_Get_Transfer_Password()
{
    return sg_sTransferPassword;
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

static struct ConfArray backupLocalitiesArray = {
    RZB_CONF_KEY_TYPE_INT,
    (void **)&sg_backupLocalities,
    &sg_backupLocalityCount,
    NULL
};


static RZBConfKey_t global_config[] = {
    // Global Items
    {"Global.MaxThreads", RZB_CONF_KEY_TYPE_INT, &sg_iThreadLimit, NULL},
    {"Global.MaxBlockSize", RZB_CONF_KEY_TYPE_INT, &sg_iMaxBlockSize, NULL},
    {"Global.HashType", RZB_CONF_KEY_TYPE_PARSED_STRING,
        &sg_iHashType, &hashCallback},
    {"Global.HelloTime", RZB_CONF_KEY_TYPE_INT, &sg_iHelloTime, NULL},
    {"Global.DeadTime", RZB_CONF_KEY_TYPE_INT, &sg_iDeadTime, NULL},
    {"Global.TransferPassword", RZB_CONF_KEY_TYPE_STRING, &sg_sTransferPassword, NULL},
    {"Global.TransferMode", RZB_CONF_KEY_TYPE_STRING, &sg_sTransferMode, NULL},

    // Local Cache Items
    {"Cache.GoodLimit", RZB_CONF_KEY_TYPE_INT, &sg_iCacheGoodLimit, NULL},
    {"Cache.BadLimit", RZB_CONF_KEY_TYPE_INT, &sg_iCacheBadLimit, NULL},

    // Message Queue Stuff
    {"MessageQueue.Host", RZB_CONF_KEY_TYPE_STRING, &sg_sMqHost, NULL},
    {"MessageQueue.Port", RZB_CONF_KEY_TYPE_INT, &sg_iMqPort, NULL},
    {"MessageQueue.User", RZB_CONF_KEY_TYPE_STRING, &sg_sMqUser, NULL},
    {"MessageQueue.Password", RZB_CONF_KEY_TYPE_STRING, &sg_sMqPassword, NULL},
    {"MessageQueue.Vhost", RZB_CONF_KEY_TYPE_STRING, &sg_sMqVhost, NULL},
    {"MessageQueue.SSL", RZB_CONF_KEY_TYPE_BOOL, &sg_sMqSSL, NULL},
    {"MessageQueue.Prefetch", RZB_CONF_KEY_TYPE_INT, &sg_iMqPrefetch, NULL},

    // Logging Stuff
    {"Log.Destination", RZB_CONF_KEY_TYPE_PARSED_STRING, &log_dest, &destCallBack},
    {"Log.Level", RZB_CONF_KEY_TYPE_PARSED_STRING, &log_level,
     &levelCallBack},
    {"Log.Syslog_Facility", RZB_CONF_KEY_TYPE_PARSED_STRING, &log_facility,
     &facilityCallBack},
    {"Log.File", RZB_CONF_KEY_TYPE_STRING, &log_file, NULL},

    {"Locality.Id", RZB_CONF_KEY_TYPE_INT, &sg_localityId, NULL},
    {"Locality.BlockStore", RZB_CONF_KEY_TYPE_STRING, &sg_localityBlockStore, NULL},
    {"Locality.BlockStoreRemote", RZB_CONF_KEY_TYPE_BOOL, &sg_blockStoreRemote, NULL},
    {"Locality.BackupOrder", RZB_CONF_KEY_TYPE_ARRAY, NULL, &backupLocalitiesArray},

    {"Inspection.Threads.Initial", RZB_CONF_KEY_TYPE_INT, &sg_inspThreadsInit, NULL},
    {"Inspection.Threads.Max", RZB_CONF_KEY_TYPE_INT, &sg_inspThreadsMax, NULL},

    {"Submission.GlobalCache.Request.Threads.Initial", RZB_CONF_KEY_TYPE_INT, &sg_subGcReqThreadsInit, NULL},
    {"Submission.GlobalCache.Request.Threads.Max", RZB_CONF_KEY_TYPE_INT, &sg_subGcReqThreadsMax, NULL},

    {"Submission.GlobalCache.Response.Threads.Initial", RZB_CONF_KEY_TYPE_INT, &sg_subGcRespThreadsInit, NULL},
    {"Submission.GlobalCache.Response.Threads.Max", RZB_CONF_KEY_TYPE_INT, &sg_subGcRespThreadsMax, NULL},

	{"Submission.Transfer.Threads.Initial", RZB_CONF_KEY_TYPE_INT, &sg_subTransferThreadsInit, NULL},
    {"Submission.Transfer.Threads.Max", RZB_CONF_KEY_TYPE_INT, &sg_subTransferThreadsMax, NULL},

    {"Submission.Pool.MaxItems", RZB_CONF_KEY_TYPE_INT, &sg_blockPoolMaxItems, NULL},
    {"Submission.Pool.MaxSize", RZB_CONF_KEY_TYPE_INT, &sg_blockPoolMaxSize, NULL},
    {"Submission.Pool.MaxItemSize", RZB_CONF_KEY_TYPE_INT, &sg_blockPoolMaxSize, NULL},




    {NULL, RZB_CONF_KEY_TYPE_END, NULL, NULL}
};

bool readApiConfig (void) {
    char *envVal;

    if (!readMyConfig (NULL, API_CONFIG_FILE, global_config)) {
        rzb_log(LOG_ERR, LOG_C_CONFIG, "Failed to read api config. Exiting.");
        return false;
    }
    sg_iMaxBlockSize = sg_iMaxBlockSize * 1024 * 1024;  // Convert to MB;

    // Check for environment variable overrides
    if ((envVal = getenv("RZB_MQ_PASSWORD")) != NULL) {
        sg_sMqPassword = strdup(envVal);
    }

    if ((sg_sMqHost == NULL)
            || (sg_sMqUser == NULL)
            || (sg_sMqPassword == NULL)
    ) {
        rzb_log(LOG_ERR, LOG_C_CONFIG, "Message Queue configuration is incomplete,");
        return false;
    }
    return true;
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
#ifdef _MSC_VER
	return true;
#else //_MSC_VER
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
#endif //_MSC_VER
    
}
