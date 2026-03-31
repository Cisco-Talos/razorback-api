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

#include "nugget_tool.h"

#include <razorback/api.h>
#include <razorback/health.h>
#include <razorback/log.h>
#include <razorback/thread.h>

#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static volatile sig_atomic_t sg_stopRequested = 0;

static void
RzbWorker_Terminate(int sig)
{
    (void)sig;
    sg_stopRequested = 1;
}

static void
RzbWorker_Usage(const char *name)
{
    fprintf(stderr,
            "Usage: %s [--debug] [--health-bind=ADDR] [--health-port=PORT] <nugget-module>\n",
            name);
}

static bool
RzbWorker_ParseHealthPort(const char *value, unsigned long *port)
{
    char *end = NULL;
    unsigned long parsed;

    if (value == NULL || port == NULL)
        return false;

    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed > UINT16_MAX)
        return false;

    *port = parsed;
    return true;
}

int
main(int argc, char **argv)
{
    static const struct option long_options[] = {
        {"debug", no_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {"health-bind", required_argument, NULL, 'b'},
        {"health-port", required_argument, NULL, 'p'},
        {NULL, 0, NULL, 0}
    };
    struct NuggetToolModule module;
    RazorbackHealthServerConfig_t healthConfig;
    const char *healthBind = "127.0.0.1";
    const char *modulePath;
    unsigned long healthPort = 0;
    int opt;
    bool debug = false;
    bool healthStarted = false;

    while ((opt = getopt_long(argc, argv, "dhb:p:", long_options, NULL)) != -1) {
        switch (opt) {
        case 'd':
            debug = true;
            break;
        case 'h':
            RzbWorker_Usage(argv[0]);
            return 0;
        case 'b':
            healthBind = optarg;
            break;
        case 'p':
            if (!RzbWorker_ParseHealthPort(optarg, &healthPort)) {
                fprintf(stderr, "%s: invalid health port: %s\n", argv[0], optarg);
                return 1;
            }
            break;
        default:
            RzbWorker_Usage(argv[0]);
            return 1;
        }
    }

    if (optind >= argc) {
        RzbWorker_Usage(argv[0]);
        return 1;
    }
    modulePath = argv[optind];

    signal(SIGINT, RzbWorker_Terminate);
    signal(SIGTERM, RzbWorker_Terminate);

    RZB_Init_API();
    if (debug)
        rzb_debug_logging();

    if (!NuggetTool_LoadModule(modulePath, &module))
        return 1;

    if (healthPort > 0) {
        healthConfig.bindAddress = healthBind;
        healthConfig.port = (uint16_t)healthPort;
        healthConfig.requireContextsForReady = true;
        if (!Razorback_Health_Start(&healthConfig)) {
            rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to start health listener", __func__);
            NuggetTool_UnloadModule(&module);
            return 1;
        }
        healthStarted = true;
    }

    if (!module.initNug()) {
        rzb_log(LOG_ERR, LOG_C_NUGGET, "%s: Nugget initialization failed for %s",
                __func__, modulePath);
        if (healthStarted)
            Razorback_Health_Stop();
        NuggetTool_UnloadModule(&module);
        return 1;
    }

    Razorback_Health_SetStartupComplete(true);
    while (!sg_stopRequested)
        Thread_Sleep(250);

    Razorback_Health_SetStartupComplete(false);
    module.shutdownNug();
    if (healthStarted)
        Razorback_Health_Stop();
    NuggetTool_UnloadModule(&module);
    return 0;
}
