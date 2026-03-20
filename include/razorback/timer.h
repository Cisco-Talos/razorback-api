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

/** @file timer.h
 * Portable timers.
 */
#ifndef RAZORBACK_TIMER_H
#define RAZORBACK_TIMER_H

#include <razorback/visibility.h>
#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque timer handle. */
struct Timer;

/**
 * Create a timer.
 * @param interval Timer interval in seconds. Must be greater than zero.
 * @param handler Function poitner to the routine to run when the timer expires.
 * @param userData Pointer to data to be passed as the argument to handler when the timer expires.
 * @return A new Timer or NULL on error.
 */
SO_PUBLIC extern struct Timer * Timer_Create(
    uint32_t interval,
    void (*handler)(void *),
    void *userData
);

/**
 * Destroy a timer.
 * @param timer The Timer to stop and destroy.
 * @return No return value.
 */
SO_PUBLIC extern void Timer_Destroy(struct Timer *timer);

#ifdef __cplusplus
}
#endif
#endif //RAZORBACK_TIMER_H
