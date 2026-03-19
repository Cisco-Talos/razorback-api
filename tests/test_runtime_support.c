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

#include <razorback/hash.h>
#include <razorback/log.h>

#include <stdarg.h>

uint32_t
Config_getHashType(void)
{
    return HASH_TYPE_SHA256;
}

void
rzb_log(unsigned level, uint64_t component, const char *fmt, ...)
{
    va_list args;

    (void)level;
    (void)component;
    (void)fmt;

    va_start(args, fmt);
    va_end(args);
}
