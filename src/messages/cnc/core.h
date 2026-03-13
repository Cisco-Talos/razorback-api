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

#ifndef RAZORBACK_MESSAGES_CNC_CORE_H
#define RAZORBACK_MESSAGES_CNC_CORE_H
#include <razorback/messages.h>

#ifdef __cplusplus
extern "C" {
#endif

void Message_CnC_Bye_Init(void);
void Message_CnC_CacheClear_Init(void);
void Message_CnC_ConfigAck_Init(void);
void Message_CnC_ConfigUpdate_Init(void);
void Message_CnC_Error_Init(void);
void Message_CnC_Go_Init(void);
void Message_CnC_Hello_Init(void);
void Message_CnC_Pause_Init(void);
void Message_CnC_Paused_Init(void);
void Message_CnC_RegReq_Init(void);
void Message_CnC_RegResp_Init(void);
void Message_CnC_Running_Init(void);
void Message_CnC_Term_Init(void);
void Message_CnC_ReReg_Init(void);
#ifdef __cplusplus
}
#endif
#endif
