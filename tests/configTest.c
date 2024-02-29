#include <stdio.h>
#include <razorback/config_file.h>

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
    printf("=== Global Valdes ===\n");
    printf("max threads: %d\n", (int)rzbconfig.maximumthreads);
    printf("bad cache size: %d\n", (int)rzbconfig.badcachesize);
    printf("good cache size: %d\n", (int)rzbconfig.goodcachesize);
    printf("url cache size: %d\n", (int)rzbconfig.urlcachesize);
    printf("network to secs: %d\n", (int)rzbconfig.network_to_secs);
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
    printf("routing type: %d\n", (int)rzbconfig.routingtype);
}

void printTestMode() {
    printf("=== Test Mode Values ===\n");
    printf("Test file dir: %s\n", rzbconfig.testing_file_dir);
}
