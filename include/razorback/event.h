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

/** #file event.h
 * Event functions.
 */
#ifndef RAZORBACK_EVENT_H
#define RAZORBACK_EVENT_H

#include <razorback/visibility.h>
#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create an EventId.
 * @return A new EventId or NULL on error.
 */
SO_PUBLIC extern struct EventId * EventId_Create(void);

/**
 * Clone an event ID.
 * @param event The EventId to clone.
 * @return A new event object or NULL on error.
 */
SO_PUBLIC extern struct EventId * EventId_Clone(struct EventId *event);

/**
 * Destroy an EventID.
 * @param event The EventId to destroy.
 * @return No return value.
 */
SO_PUBLIC extern void EventId_Destroy(struct EventId *event);

/**
 * Create an Event.
 * @return A new event or NULL on error.
 */
SO_PUBLIC extern struct Event * Event_Create(void);

/**
 * Destroy an Event.
 * @param event The Event to destroy.
 * @return No return value.
 */
SO_PUBLIC extern void Event_Destroy(struct Event *event);

#ifdef __cplusplus
}
#endif
#endif
