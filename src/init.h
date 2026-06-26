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

#ifndef RAZORBACK_INIT_H
#define RAZORBACK_INIT_H
#include <razorback/types.h>
#ifdef __cplusplus
extern "C" {
#endif
// log.c
extern bool configureLogging (void);

// local_cache.c
extern void initcache (void);

// uuids.c
extern void initUuids (void);

// runtime_config.c
bool readApiConfig (void);

// api.c
void initApi (void);

// thread.c
bool Thread_Initialize(void);

bool Crypto_Initialize(void);
bool Socket_TLS_InitializeSharedState(void);

//magic.c
bool Magic_Init(void);

//messages/core.c
bool Message_Init(void);

#ifdef __cplusplus
}
#endif
#endif
