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

/** @file file.h
 * File handling functions.
 */
#ifndef RAZORBACK_FILE_H
#define RAZORBACK_FILE_H

#include <razorback/types.h>
#include <razorback/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Delete a file from the block store.
 * @param block The block to remove.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool File_Delete(struct Block *block);

#ifdef __cplusplus
}
#endif
#endif
