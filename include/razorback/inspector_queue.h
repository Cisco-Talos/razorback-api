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

/** @file inspector_queue.h
 * InspectorQueue functions
 */

#ifndef RAZORBACK_INSPECTORQUEUE_H
#define RAZORBACK_INSPECTORQUEUE_H

#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/messages.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the inspector queue.
 * @param p_pApplicationType the application type.
 * @param p_iFlags flags.
 * @return a pointer to the Queue object or NULL on an error.
 */
SO_PUBLIC extern struct Queue * InspectorQueue_Initialize(uuid_t p_pApplicationType, int p_iFlags);

/**
 * Terminates the inspector queue.
 * @param p_pApplicationType Application type identifier.
 * @return No return value.
 */
SO_PUBLIC extern void InspectorQueue_Terminate(uuid_t p_pApplicationType);

#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_INSPECTORQUEUE_H
