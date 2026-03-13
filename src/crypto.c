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

#include <razorback/log.h>
#include <razorback/lock.h>
#include <razorback/thread.h>
#include "init.h"


#include <openssl/ssl.h>
#include <openssl/err.h>




static bool Crypto_Initialize_OpenSSL(void)
{

    SSL_load_error_strings ();
    SSL_library_init();
    OpenSSL_add_all_digests();
    return true;
}

bool Crypto_Initialize(void)
{
	return Crypto_Initialize_OpenSSL();
}
