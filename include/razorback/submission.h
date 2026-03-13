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

/** Submission API.
 * @file submission.h
 */
#ifndef RAZORBACK_SUBMISSION_H
#define RAZORBACK_SUBMISSION_H

#include <razorback/visibility.h>
#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RZB_SUBMISSION_OK 0
#define RZB_SUBMISSION_ERROR 1
#define RZB_SUBMISSION_NO_TYPE 2

/** Submit a block pool item.
 * @param p_pItem The item.
 * @param p_iFlags The submission flags.
 * @param p_pSf_Flags Pointer to a uint32_t to store the SourceFire threat flags in.
 * @param p_pEnt_Flags Pointer to a uint32_t to store the Enterprise threat flags in.
 * @return true on success false on error.
 */
SO_PUBLIC extern int Submission_Submit(struct BlockPoolItem *p_pItem, int p_iFlags, uint32_t *p_pSf_Flags, uint32_t *p_pEnt_Flags);
#ifdef __cplusplus
}
#endif
#endif
