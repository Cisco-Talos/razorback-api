#include "config.h"

#include "init.h"
#include <curl/curl.h>

static bool initCurl() {
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        fprintf(stderr, "Failed to initialize libcurl\n");
        exit(1);
    }
    return true;
}
#ifdef _MSC_VER
#include "safewindows.h"
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <locale.h>
static WORD wVersionRequested;
static WSADATA wsaData;

BOOL WINAPI DllMain(
  __in  HINSTANCE hinstDLL,
  __in  DWORD fdwReason,
  __in  LPVOID lpvReserved
)
{
	switch( fdwReason ) 
    { 
    case DLL_PROCESS_ATTACH:
		wVersionRequested = MAKEWORD(2, 2);
		WSAStartup(wVersionRequested, &wsaData);
		setlocale( LC_ALL, "C" );
#else
SO_PUBLIC void __attribute__ ((constructor)) 
RZB_Init_API ()
{
#endif
		Crypto_Initialize();
		initCurl();
		readApiConfig();
		configureLogging();
		Magic_Init();
		initcache();
		initUuids();
		initApi();
		Message_Init();
		if (!Transfer_Init()) {
            exit(1);
        }
#ifdef _MSC_VER
		break;
	default:
		break;
	}
	return true;
#endif
}

