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

#include <razorback/config_file.h>
#include <razorback/log.h>
#include "runtime_config.h"

#define API_CONFIG_FILE "rzb.conf"

// Libconfig handle
static config_t config;


typedef enum
{
    UNINIT, READY, CONFIGURED, UNCONFIGURED
} CONFIGSTATE;

struct RZBConfigFile
{
    RZBConfKey_t *type;
    config_t config;
    struct RZBConfigFile *next;
};

struct RZBConfigFile;
typedef struct RZBConfigFile RZBConfigFile;

RZBConfigFile *configList = NULL;

CONFIGSTATE configState = UNCONFIGURED;

bool testFile (const char *);
bool parseBlock (config_t *, RZBConfKey_t *);
static char *getConfigFile (const char *, const char *);
bool parseRoutingType (const char *, conf_int_t *);

static char *
getConfigFile (const char *configDir, const char *configFile)
{
    if (configDir == NULL)
    {
        configDir = ETC_DIR;
    }
    char *result;
    result =
        calloc (strlen (configDir) + strlen (configFile) + 2, sizeof (char));
    if (result == NULL)
        return NULL;

    strncpy (result, configDir, strlen (configDir) + 1);
    strncat (result, "/", 1);
    strncat (result, configFile, strlen (configFile));

    return result;
}


SO_PUBLIC bool
readMyConfig (const char *configDir, const char *configFile,
              RZBConfKey_t * config_fmt)
{
    if (configFile == NULL)
    {
        rzb_log (LOG_EMERG, "readMyConfigFile configFile was null");
        return false;
    }
    if (configState == UNINIT)
    {
        memset (&config, 0, sizeof (config));
        config_init (&config);
        configState = READY;
    }

    char *configfile = getConfigFile (configDir, configFile);
    RZBConfigFile * file;

    if ((file =calloc(1, sizeof (RZBConfigFile))) == NULL)
    {
        rzb_log(LOG_ERR, "Failed to allocate config storage");
        free(configfile);
        return false;
    }

    file->type = config_fmt;
    if (!testFile (configfile))
    {
        free (configfile);
        return false;
    }
    if (config_read_file (&file->config, configfile) != CONFIG_TRUE)
        rzb_log (LOG_ERR, "%s\n", config_error_text (&config));

    if (configList == NULL)
    {
        configList = file;
    }
    else
    {
        file->next = configList;
        configList = file;
    }
    free (configfile);
    return parseBlock (&file->config, config_fmt);
}

bool
parseBlock (config_t * config, RZBConfKey_t * block)
{
    int status = CONFIG_TRUE;
    conf_int_t t;
    config_setting_t *tt;
    const char *type;
    while (block->type != RZB_CONF_KEY_TYPE_END)
    {
        tt = config_lookup (config, block->key);
        if (tt == NULL)
        {
            rzb_log (LOG_WARNING, "Cant find key: %s", block->key);
            block++;
            continue;
        }

        if (block->type == RZB_CONF_KEY_TYPE_INT)
        {
            status = config_lookup_int (config, block->key, &t);
            *(conf_int_t *) (block->dest) = t;
        }
        else if (block->type == RZB_CONF_KEY_TYPE_STRING)
        {
            status = config_lookup_string (config, block->key, block->dest);
        }
        else if (block->type == RZB_CONF_KEY_TYPE_PARSED_STRING)
        {
            status = config_lookup_string (config, block->key, &type);
            if (status != CONFIG_TRUE)
            {
                rzb_log (LOG_ERR, "%s\n", config_error_text (config));
                return false;
            }
            if (!block->callback->parseString (type, &t))
                return false;

            *(int *) (block->dest) = (int) t;
        }
        else if (block->type == RZB_CONF_KEY_TYPE_UUID)
        {
            status = config_lookup_string (config, block->key, &type);
            if (status != CONFIG_TRUE)
            {
                rzb_log (LOG_ERR, "%s\n", config_error_text (config));
                return false;
            }
            if (uuid_parse (type, block->dest) == -1)
            {
                rzb_log (LOG_ERR, "Failed to parse UUID: %s\n", type);
                return false;
            }

        }
        else
        {
            rzb_log (LOG_ERR, "Unknown config attribute type.");
            return false;
        }
        if (status != CONFIG_TRUE)
        {
            rzb_log (LOG_ERR, "%s\n", config_error_text (config));
            return false;
        }
        block++;
    }
    return true;
}

bool
parseRoutingType (const char *string, conf_int_t * val)
{
    if (!strncasecmp (string, "opaque", 6))
    {
        *val = 0;
        return true;
    }
    else if (!strncasecmp (string, "transparent", 11))
    {
        *val = 1;
        return true;
    }
    return false;

}

bool
testFile (const char *configfile)
{
    struct stat sb;
    int fd = open (configfile, O_RDONLY);

    if (fd == -1)
    {
        rzb_log (LOG_EMERG, "Failed to open (%s) in ", configfile);
        return false;
    }

    if (fstat (fd, &sb) == -1)
    {
        return false;
    }
    close (fd);
    return true;

}

SO_PUBLIC void
rzbConfCleanUp (void)
{
    config_destroy (&config);
    while (configList != NULL)
    {
        config_destroy (&configList->config);
        configList = configList->next;
    }
}

config_setting_t *
getConfigArray (config_t *config, const char *configFile, const char *name)
{
    config_setting_t *list;

	if (config_read_file (config, configFile) != CONFIG_TRUE) 
	{
		printf("HereA\n");
		rzb_log (LOG_ERR, "%s\n", config_error_text (config));
		return NULL;
	}

	if ((list = config_lookup (config, name)) == NULL)
	{
		printf("HereB\n");
		rzb_log (LOG_ERR, "%s\n", config_error_text (config));
		return NULL;
	}

	return list;
}

config_setting_t *
getNextConfigElem (config_t *config, config_setting_t *list)
{
    config_setting_t *configSetting;
    static unsigned int currItem = 0;
    unsigned int numItems = config_setting_length (list);
    
	while (currItem < numItems)
    {
        if ((configSetting = config_setting_get_elem (list,
                                      currItem)) != NULL)
		{
            currItem++;
			return configSetting;
		}
		else {
            rzb_log (LOG_ERR, "%s\n", config_error_text (config));
			break;
		}
	}

	numItems = 0;
	currItem = 0;
	return NULL;
}
