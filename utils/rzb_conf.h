/** @file rzb_conf.h
 * Razorback Configuration API
 */
#ifndef RZB_CONF_H
#define RZB_CONF_H

#include "rzb_utils_types.h"


/** The global configuration stucture for the API.
 */
typedef struct _RZBConfig
{
//Global
    unsigned maximumthreads;    ///< Maximum number of threads the API will spawn.
    unsigned badcachesize;      ///< The number of blocks cached with a deposision of bad.
    unsigned goodcachesize;     ///< The number of blocks cached with a deposision of good.
    unsigned urlcachesize;      ///< The size of the URL cache.
    unsigned nuggettype;        ///< The type of the nugget.
    unsigned network_to_secs;   ///< The network timeout in seconds.
//Nugget Server
    char *nugname;              ///< The name of the nugget.
    char *nugport;              ///< The port to contact this nugget on.
    char *nugaddr;              ///< The IP Address of this nugget.
    char *handlerdir;           ///< The path to the loadable .so nuggets
//Dispatch Server
    char *dsrvaddr;             ///< The IP address of the dispatcher
    char *dsrvport;             ///< The port to contact the dispatcher on.
    int routingtype;            ///< The type of routing to be used by the dispatcher.
// Test mode
    char *testing_file_dir;     ///< The path to write test files to.

} RZBConfig;

/** Avaliable types to read from the config file.
 */
typedef enum
{
    RZB_CONF_KEY_TYPE_STRING = 5,   ///< A String.
    RZB_CONF_KEY_TYPE_SIGNED,       ///< A signed int.
    RZB_CONF_KEY_TYPE_UNSIGNED,     ///< An unsinged int.
    RZB_CONF_KEY_TYPE_ROUTING_TYPE, ///< A routing type string.
    RZB_CONF_KEY_TYPE_END           ///< End of block marker.
} RZB_CONF_KEY_TYPE_t;

/**
 * Configuration file entry definition.
 */
typedef struct {
    const char* key;            ///< The path to the entry in the config file.
    RZB_CONF_KEY_TYPE_t type;   ///< The ::RZB_CONF_KEY_TYPE_t value for the type of value to read for the entry.
    void *dest;                 ///< A pointer to the pointer to the memory to be used to store the value.
} RZBConfKey_t;

/** Read the API configuration from from rzb.conf located in configDir.
 * @param *configDir The name of the directory to read the config file from.
 * @return a value from ::HRESULT
 */
HRESULT readApiConfig(const char *configDir);

/** Read a component configuration file.
 * @param *configDir the dir to look in
 * @param *configFile the file read
 * @param *config the structure of the file.
 * @return a value from ::HRESULT
 */
HRESULT readMyConfig(const char *configDir, const char *configFile, RZBConfKey_t *config);

/** Clean the memory allocated by ::readApiConfig and ::readMyConfig
 *
 */
void rzbConfCleanUp();

extern RZBConfig rzbconfig;

#endif

 /** @example read_config.c
  * The following example shows how to use ::readMyConfig
  */

