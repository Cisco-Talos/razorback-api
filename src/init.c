#include "config.h"

#include "init.h"

SO_PUBLIC void __attribute__ ((constructor)) 
RZB_Init_API ()
{
    Crypto_Initialize();
    readApiConfig();
    configureLogging();
    Magic_Init();
    initcache();
    initUuids();
    initApi();
}

