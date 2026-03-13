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

/** @file judgment.h
 * Judgment transmission functions.
 */
#ifndef RAZORBACK_JUDGMENT_H
#define RAZORBACK_JUDGMENT_H

#include <razorback/visibility.h>
#include <razorback/api.h>
#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

SO_PUBLIC extern struct Judgment * Judgment_Create (struct EventId *eventId, struct BlockId *blockId);
SO_PUBLIC extern void Judgment_Destroy (struct Judgment *judgment);

/** Render a verdict on a block
 */
SO_PUBLIC extern bool
Judgment_Render_Verdict (uint8_t p_iLevel, struct Judgment *p_pJudgment);
#ifdef __cplusplus
}
#endif
#endif
