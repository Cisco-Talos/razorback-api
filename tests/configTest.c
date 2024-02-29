#include <stdio.h>
#include <rzb_conf.h>

void printGlobal();
void printNuggetServer();
void printDispatchServer();
void printTestMode();

int main() {
    readApiConfig(NULL);
    printGlobal();
    printNuggetServer();
    printDispatchServer();
    printTestMode();
}

void printGlobal() {
    printf("=== Global Values ===\n");
    printf("max threads: %u\n", rzbconfig.maximumthreads);
    printf("bad cache size: %u\n", rzbconfig.badcachesize);
    printf("good cache size: %u\n", rzbconfig.goodcachesize);
    printf("url cache size: %u\n", rzbconfig.urlcachesize);
    printf("network to secs: %u\n", rzbconfig.network_to_secs);
}

void printNuggetServer() {
    printf("=== Nugget Server Values ===\n");
    printf("Nugname: %s\n", rzbconfig.nugname);
    printf("Nugport: %s\n", rzbconfig.nugport);
    printf("Nugaddr: %s\n", rzbconfig.nugaddr);
    printf("Handledir: %s\n", rzbconfig.handlerdir);
}

void printDispatchServer() {
    printf("=== Dispatch Server Values ===\n");
    printf("dsrvaddr: %s\n", rzbconfig.dsrvaddr);
    printf("dsrvport: %s\n", rzbconfig.dsrvport);
    printf("routing type: %u\n", rzbconfig.routingtype);
}

void printTestMode() {
    printf("=== Test Mode Values ===\n");
    printf("Test file dir: %s\n", rzbconfig.testing_file_dir);
}
