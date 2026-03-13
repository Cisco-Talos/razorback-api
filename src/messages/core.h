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

#ifndef RAZORBACK_MESSAGES_CORE_H
#define RAZORBACK_MESSAGES_CORE_H
#include <razorback/messages.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Message * Message_Create(uint32_t type, uint32_t version, size_t msgSize);
void Message_Destroy(struct Message *message);
bool Message_Setup(struct Message *message);
struct MessageHeader * Message_HeaderList_Add(List_t * headers, const char *p_sName, const char *p_sValue );
void MessageBlockSubmission_Init(void);
void MessageCacheReq_Init(void);
void MessageCacheResp_Init(void);
void MessageInspectionSubmission_Init(void);
void MessageJudgmentSubmission_Init(void);
void MessageLogSubmission_Init(void);
void MessageLogSubmission_Init(void);
void MessageAlertPrimary_Init(void);
void MessageAlertChild_Init(void);
void MessageOutputLog_Init(void);
void MessageOutputEvent_Init(void);

#ifdef __cplusplus
}
#endif

#endif
