/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "config.h"

#include "init.h"
#include "telemetry.h"
#include <curl/curl.h>
#include <razorback/log.h>
#include <razorback/visibility.h>
#include <stdlib.h>


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
        if (!readApiConfig()) {
            exit(1);
        }
        configureLogging();
        if (!Telemetry_Initialize()) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: OpenTelemetry initialization failed, continuing without tracing",
                    __func__);
        }
        atexit(Telemetry_Shutdown);
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
