#include <stdio.h>
#include <razorback/config_file.h>
#include <razorback/log.h>


int main() {
    rzb_log(LOG_EMERG, LOG_C_CORE, "Emergency test");
    rzb_log(LOG_ALERT,LOG_C_CORE, "Alert test");
    rzb_log(LOG_CRIT,LOG_C_CORE, "Critical test");
    rzb_log(LOG_ERR,LOG_C_CORE, "Error test");
    rzb_log(LOG_WARNING,LOG_C_CORE, "Warning test");
    rzb_log(LOG_NOTICE,LOG_C_CORE, "Notice test");
    rzb_log(LOG_INFO,LOG_C_CORE, "Info test");
    rzb_log(LOG_DEBUG,LOG_C_CORE, "Debug test");

}


