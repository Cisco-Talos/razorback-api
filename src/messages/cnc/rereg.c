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

#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/log.h>

#include "messages/core.h"
#include "messages/cnc/core.h"

SO_PUBLIC struct Message *
MessageReReg_Initialize (
                         const uuid_t p_uuidSourceNugget,
                         const uuid_t p_uuidDestNugget)
{
    struct Message *msg;
    msg = Message_Create_Directed(
        MESSAGE_TYPE_REREG,
        MESSAGE_VERSION_1,
        0,
        p_uuidSourceNugget,
        p_uuidDestNugget
    );
    if (msg == NULL) {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Message_Create_Directed failed", __func__);
        return NULL;
    }

    msg->destroy = Message_Destroy;
    msg->deserialize = Message_Deserialize_Empty;
    msg->serialize = Message_Serialize_Empty;
    return msg;
}

static struct MessageHandler handler = {
    MESSAGE_TYPE_REREG,
    Message_Serialize_Empty,
    Message_Deserialize_Empty,
    Message_Destroy
};

// core.h
void 
Message_CnC_ReReg_Init(void)
{
    Message_Register_Handler(&handler);
}
