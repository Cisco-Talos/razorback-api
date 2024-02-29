#include "config.h"

#include "rzb_utils.h"

SO_PUBLIC int rzb_debug = 0;

SO_PUBLIC void rzbSetDebugMode(int mode)
{
    rzb_debug = mode;
}

