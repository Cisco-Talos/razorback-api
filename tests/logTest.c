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


