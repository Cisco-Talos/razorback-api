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

#ifndef RAZORBACK_DEV_MODE_H
#define RAZORBACK_DEV_MODE_H

#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Razorback_DevMode_Initialize(void);
void Razorback_DevMode_SetEnabled(bool enabled);
bool Razorback_DevMode_IsEnabled(void);

bool Razorback_DevMode_RegisterContext(struct RazorbackContext *context);
void Razorback_DevMode_UnregisterContext(struct RazorbackContext *context);

bool Razorback_DevMode_CaptureVerdict(struct RazorbackContext *context,
                                      const struct Judgment *judgment);
bool Razorback_DevMode_CaptureSubmission(struct RazorbackContext *context,
                                         struct BlockPoolItem *item);

List_t *Razorback_DevMode_GetJudgments(struct RazorbackContext *context);
List_t *Razorback_DevMode_GetSubmissions(struct RazorbackContext *context);

#ifdef __cplusplus
}
#endif

#endif
