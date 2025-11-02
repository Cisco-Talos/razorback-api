#include <unistd.h>
#include <stdio.h>
#include <razorback/config_file.h>
#include <razorback/log.h>
#include <razorback/daemon.h>


int main() {
    rzb_log(LOG_INFO, LOG_C_CORE,"Daemonizing");
    if (rzb_daemonize(NULL, NULL)) {
        rzb_log(LOG_EMERG, LOG_C_CORE,"Failed to daemonize");
        return 1;
    }
    rzb_log(LOG_INFO,LOG_C_CORE, "Sleeping as a daemon");
    sleep(60);
    rzb_log(LOG_INFO,LOG_C_CORE, "Daemon exiting");
}


