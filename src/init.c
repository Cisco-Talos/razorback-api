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
#include "block_pool_private.h"
#include "health_internal.h"
#include "submission_private.h"
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

static bool initWinsock(void)
{
    wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
        fprintf(stderr, "Failed to initialize WinSock\n");
        exit(1);
    }
    setlocale(LC_ALL, "C");
    return true;
}
#endif

SO_PUBLIC void
RZB_Init_API(void)
{
#ifdef _MSC_VER
    initWinsock();
#endif
    Crypto_Initialize();
    initCurl();
    if (!readApiConfig()) {
        exit(1);
    }
    configureLogging();
    if (!Telemetry_Initialize()) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: OpenTelemetry initialization failed, continuing without telemetry",
                __func__);
    }
    atexit(Telemetry_Shutdown);
    Magic_Init();
    initcache();
    initUuids();
    initApi();
    if (!Health_Initialize()) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize health subsystem, continuing without health",
                __func__);
    } else {
        atexit(Health_Shutdown_Global);
    }
    if (!BlockPool_Init()) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize block pool", __func__);
        exit(1);
    }
    if (!Submission_Initialize()) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize submission state", __func__);
        exit(1);
    }
    atexit(Submission_Shutdown_Global);
    Message_Init();
    if (!Transfer_Init()) {
        exit(1);
    }
}
