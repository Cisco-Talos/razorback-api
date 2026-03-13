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


