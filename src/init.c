#include "config.h"

#include "init.h"

#ifdef _MSC_VER
#define CONSTRUCTOR
#else
#define CONSTRUCTOR __attribute__ ((constructor)) 
#endif
SO_PUBLIC void CONSTRUCTOR
RZB_Init_API ()
{
    Crypto_Initialize();
    readApiConfig();
    configureLogging();
    Magic_Init();
    initcache();
    initUuids();
    initApi();
    Message_Init();
    Transfer_Init();
}

