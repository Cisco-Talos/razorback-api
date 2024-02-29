#ifndef RAZORBACK_INIT_H
#define RAZORBACK_INIT_H
#include <razorback/types.h>

// log.c
extern bool configureLogging (void);

// local_cache.c
extern void initcache (void);

// uuids.c
extern void initUuids (void);

// runtime_config.c
void readApiConfig (void);

// api.c
void initApi (void);
#endif
