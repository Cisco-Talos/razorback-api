#ifndef RAZORBACK_RUNTIME_CONFIG_H
#define RAZORBACK_RUNTIME_CONFIG_H
#include <razorback/log.h>
#include <razorback/types.h>

extern uint32_t Config_getHashType (void);

extern uint32_t Config_getMaxBlockSize (void);

extern uint32_t Config_getThreadLimit (void);
extern uint32_t Config_getHelloTime (void);
extern uint32_t Config_getDeadTime (void);
extern uint32_t Config_getLocality (void);

extern char *Config_getMqHost (void);

extern uint32_t Config_getMqPort (void);

extern char *Config_getMqUser (void);

extern char *Config_getMqPassword (void);
extern bool Config_getMqSSL (void);

extern uint32_t Config_getCacheGoodLimit (void);

extern uint32_t Config_getCacheBadLimit (void);


void Config_setLogDest (RZB_LOG_DEST_t);
void Config_setLogLevel(int);
RZB_LOG_DEST_t Config_getLogDest (void);
int Config_getLogLevel(void);
int Config_getLogFacility(void);
char * Config_getLogFile(void);
#endif
