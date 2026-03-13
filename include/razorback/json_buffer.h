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

/** JSON Buffer Implimentation 
 */
#ifndef RAZORBACK_JSON_BUFFER_H
#define RAZORBACK_JSON_BUFFER_H

#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/messages.h>
#include <razorback/ntlv.h>

#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

SO_PUBLIC extern bool JsonBuffer_Put_bool (json_object * parent,
                                              const char *name, bool p_iValue);
SO_PUBLIC extern bool JsonBuffer_Put_uint8_t (json_object * parent,
                                    const char *name, uint8_t p_iValue);

SO_PUBLIC extern bool JsonBuffer_Put_uint16_t (json_object * parent, const char * name, uint16_t p_iValue);

SO_PUBLIC extern bool JsonBuffer_Put_uint32_t (json_object * parent, const char * name, uint32_t p_iValue);

SO_PUBLIC extern bool JsonBuffer_Put_uint64_t (json_object * parent, const char * name, uint64_t p_iValue);

SO_PUBLIC extern bool JsonBuffer_Put_ByteArray (json_object * parent, const char *name, 
                                      uint32_t p_iSize,
                                      const uint8_t * p_pByteArray);

SO_PUBLIC extern bool JsonBuffer_Put_String (json_object * parent, const char * name,
                                   const char * p_sString);

SO_PUBLIC extern bool JsonBuffer_Get_bool (json_object * parent, const char * name, bool * p_pValue);
SO_PUBLIC extern bool JsonBuffer_Get_uint8_t (json_object * parent, const char * name, uint8_t * p_pValue);

SO_PUBLIC extern bool JsonBuffer_Get_uint16_t (json_object * parent, const char * name,
                                     uint16_t * p_pValue);

SO_PUBLIC extern bool JsonBuffer_Get_uint32_t (json_object * parent, const char *name,
                                     uint32_t * p_pValue);

SO_PUBLIC extern bool JsonBuffer_Get_uint64_t (json_object * parent, const char *name,
                                     uint64_t * p_pValue);

SO_PUBLIC extern char *JsonBuffer_Get_String (json_object * parent, const char * name);

SO_PUBLIC extern bool JsonBuffer_Get_ByteArray (json_object * parent, const char * name,
                                      uint32_t * p_iSize,
                                      uint8_t ** p_pByteArray);

SO_PUBLIC extern bool JsonBuffer_Get_UUID (json_object * parent, const char *name, uuid_t p_uuid);
SO_PUBLIC extern bool JsonBuffer_Put_UUID (json_object * parent, const char * name, uuid_t p_uuid);

SO_PUBLIC extern bool JsonBuffer_Put_NTLVList (json_object * parent, const char * name,
                                     List_t *p_pList);

SO_PUBLIC extern bool JsonBuffer_Get_NTLVList (json_object * parent, const char * name,
                                     List_t **p_pList);

SO_PUBLIC extern bool JsonBuffer_Put_Hash (json_object * parent, const char * name,
                                 const struct Hash *p_pHash);

SO_PUBLIC extern bool JsonBuffer_Get_Hash (json_object * parent, const char *name, struct Hash **p_pHash);

SO_PUBLIC extern bool JsonBuffer_Put_BlockId (json_object * parent, const char *name,
                                    struct BlockId *p_pId);


SO_PUBLIC extern bool JsonBuffer_Get_BlockId (json_object * parent, const char * name,
                                    struct BlockId **p_pId);

SO_PUBLIC extern bool JsonBuffer_Put_Block (json_object * parent, const char * name, 
                                  struct Block *p_pBlock);

SO_PUBLIC extern bool JsonBuffer_Get_Block (json_object * parent, const char * name,
                                  struct Block **p_pBlock);

SO_PUBLIC extern bool JsonBuffer_Put_Event (json_object * parent, const char * name, 
                                  struct Event *p_pEvent);

SO_PUBLIC extern bool JsonBuffer_Get_Event (json_object * parent, const char * name,
                                  struct Event **p_pEvent);


SO_PUBLIC extern bool JsonBuffer_Put_EventId (json_object * parent, const char * name,
                                    struct EventId *p_pEventId);

SO_PUBLIC extern bool JsonBuffer_Get_EventId (json_object * parent, const char * name, 
                                    struct EventId **p_pEventId);

SO_PUBLIC extern bool JsonBuffer_Put_Judgment (json_object * parent, const char * name,
                                     struct Judgment *p_pJudgment);

SO_PUBLIC extern bool JsonBuffer_Get_Judgment (json_object * parent, const char * name, 
                                     struct Judgment **p_pJudgment);

SO_PUBLIC extern bool JsonBuffer_Put_Nugget (json_object * parent, const char * name, 
                                           struct Nugget * nugget);

SO_PUBLIC extern bool JsonBuffer_Get_Nugget (json_object * parent, const char * name, 
                                           struct Nugget ** r_nugget);

SO_PUBLIC extern bool JsonBuffer_Put_UUIDList (json_object * parent, const char * name, 
                                           List_t * list);
SO_PUBLIC extern bool JsonBuffer_Get_UUIDList (json_object * parent, const char * name, 
                                           List_t ** r_list);

SO_PUBLIC extern bool JsonBuffer_Put_StringList (json_object * parent, const char * name, 
                                           List_t * list);
SO_PUBLIC extern bool JsonBuffer_Get_StringList (json_object * parent, const char * name, 
                                           List_t ** r_list);
SO_PUBLIC extern bool 
JsonBuffer_Put_uint8List (json_object * parent, const char * name, 
                                           uint8_t *list, uint32_t count);
SO_PUBLIC extern bool 
JsonBuffer_Get_uint8List (json_object * parent, const char * name, 
                                           uint8_t **list, uint32_t *count);
#ifdef __cplusplus
}
#endif
#endif //RAZORBACK_JSON_BUFFER_H
