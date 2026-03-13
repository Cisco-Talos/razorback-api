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
static bool Crypto_Initialize_OpenSSL(void)
{
    if (OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT, NULL) != 1)
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize OpenSSL", __func__);
        return false;
    }
    if (!Socket_TLS_InitializeSharedState())
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to initialize shared TLS state", __func__);
        return false;
    }
    return true;
}

bool Crypto_Initialize(void)
{
    return Crypto_Initialize_OpenSSL();
}
