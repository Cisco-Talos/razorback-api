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

#ifndef RAZORBACK_SAFEWINDOWS_H
#define RAZORBACK_SAFEWINDOWS_H

#if !defined __CYGWIN__ && !defined _MSC_VER
# error Including win32/safewindows.h, but neither __CYGWIN__ nor _MSC_VER defined!
#endif

// Prevent windows.h from defining min and max macros.
#ifndef NOMINMAX
# define NOMINMAX
#endif

// Prevent windows.h from including lots of obscure win32 api headers
// which we don't care about and will just slow down compilation and
// increase the risk of symbol collisions.
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <Windows.h>
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#endif // RAZORBACK_SAFEWINDOWS_H