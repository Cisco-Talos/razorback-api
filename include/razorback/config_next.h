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

/** @file config_next.h
 * Dispatcher-next YAML/env configuration helper.
 */
#ifndef RAZORBACK_CONFIG_NEXT_H
#define RAZORBACK_CONFIG_NEXT_H

#include <razorback/types.h>
#include <razorback/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

enum RzbNextConfigType
{
    RZB_NEXT_CONFIG_STRING = 0,
    RZB_NEXT_CONFIG_INT = 1,
    RZB_NEXT_CONFIG_BOOL = 2
};

struct RzbNextConfigKey
{
    const char *key;
    enum RzbNextConfigType type;
    void *dest;
};

SO_PUBLIC extern bool RzbNextConfig_Load(
    const char *baseFile,
    const char *localFile,
    const char *envPrefix,
    const struct RzbNextConfigKey *keys
);

#ifdef __cplusplus
}
#endif
#endif /* RAZORBACK_CONFIG_NEXT_H */
