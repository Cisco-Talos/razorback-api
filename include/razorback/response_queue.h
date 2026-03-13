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

/** @file response_queue.h
 * ResponseQueue functions
 */

#ifndef RAZORBACK_RESPONSEQUEUE_H
#define RAZORBACK_RESPONSEQUEUE_H

#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/messages.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the response queue.
 * @param p_pCollectorId the queue to add it to.
 * @param p_iFlags falgs.
 * @return a pointer to the queue or null on error.
 */
SO_PUBLIC extern struct Queue * ResponseQueue_Initialize(uuid_t p_pCollectorId, int p_iFlags);

/**
 * Terminates the response queue.
 * @param p_pCollectorId Collector identifier.
 * @return No return value.
 */
SO_PUBLIC extern void ResponseQueue_Terminate(uuid_t p_pCollectorId);

#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_RESPONSEQUEUE_H
