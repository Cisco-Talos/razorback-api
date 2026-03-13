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

#include <rzb_conf.h>

const char *my_string;

static RZBConfKey_t my_config[] = {
    { "MY_STRING", RZB_CONF_KEY_TYPE_STRING, &my_string },
    { NULL, RZB_CONF_KEY_TYPE_END, NULL }
};

if (readApiConfig(NULL) != R_SUCCESS)
    printf("Failed to load api config file\n");

if (readMyConfig(NULL, "rzb_custom.conf", my_config) != R_SUCCESS)
    printf("Failed to load custom config file\n");
else
    printf("MY_STRING: %s\n", my_string);

