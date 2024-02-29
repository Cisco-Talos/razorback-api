#include "config.h"

#include "init.h"

SO_PUBLIC void __attribute__ ((constructor)) 
RZB_Init_API ()
{
    configureLogging();
    initcache();
    initUuids();
}

