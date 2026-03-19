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

/**
 * Add a boolean value to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_iValue Value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_bool(json_object * parent, const char *name, bool p_iValue);

/**
 * Add an 8-bit unsigned integer value to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_iValue Value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_uint8_t(
    json_object * parent,
    const char *name,
    uint8_t p_iValue
);

/**
 * Add a 16-bit unsigned integer value to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_iValue Value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_uint16_t(
    json_object * parent,
    const char * name,
    uint16_t p_iValue
);

/**
 * Add a 32-bit unsigned integer value to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_iValue Value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_uint32_t(
    json_object * parent,
    const char * name,
    uint32_t p_iValue
);

/**
 * Add a 64-bit unsigned integer value to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_iValue Value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_uint64_t(
    json_object * parent,
    const char * name,
    uint64_t p_iValue
);

/**
 * Add a byte array to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_iSize Size value.
 * @param p_pByteArray Byte array value.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_ByteArray(
    json_object * parent,
    const char *name,
    uint32_t p_iSize,
    const uint8_t * p_pByteArray
);

/**
 * Add a string value to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_sString String value.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_String(
    json_object * parent,
    const char * name,
    const char * p_sString
);

/**
 * Add an opaque JSON object or array value to a JSON object from its string form.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_sJsonString String containing a serialized JSON object or array.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_JsonString(
    json_object * parent,
    const char * name,
    const char * p_sJsonString
);

/**
 * Get a boolean value from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pValue Value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_bool(json_object * parent, const char * name, bool * p_pValue);

/**
 * Get an 8-bit unsigned integer value from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pValue Value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_uint8_t(
    json_object * parent,
    const char * name,
    uint8_t * p_pValue
);

/**
 * Get a 16-bit unsigned integer value from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pValue Value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_uint16_t(
    json_object * parent,
    const char * name,
    uint16_t * p_pValue
);

/**
 * Get a 32-bit unsigned integer value from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pValue Value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_uint32_t(
    json_object * parent,
    const char *name,
    uint32_t * p_pValue
);

/**
 * Get a 64-bit unsigned integer value from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pValue Value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_uint64_t(
    json_object * parent,
    const char *name,
    uint64_t * p_pValue
);

/**
 * Get a string value from a JSON object.
 * @param parent Parent object.
 * @param name Name of the JSON field to read.
 * @return Allocated string on success, or NULL on failure.
 */
SO_PUBLIC extern char * JsonBuffer_Get_String(json_object * parent, const char * name);

/**
 * Get an opaque JSON object or array value from a JSON object in its string form.
 * @param parent Parent object.
 * @param name Name of the JSON field to read.
 * @return Allocated compact JSON string on success, or NULL on failure.
 */
SO_PUBLIC extern char * JsonBuffer_Get_JsonString(json_object * parent, const char * name);

/**
 * Get a byte array from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_iSize Size value.
 * @param p_pByteArray Destination for byte array.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_ByteArray(
    json_object * parent,
    const char * name,
    uint32_t * p_iSize,
    uint8_t ** p_pByteArray
);

/**
 * Get a UUID value from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_uuid Destination UUID value.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_UUID(json_object * parent, const char *name, uuid_t p_uuid);

/**
 * Add a UUID value to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_uuid UUID value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_UUID(json_object * parent, const char * name, uuid_t p_uuid);

/**
 * Add an NTLV list to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pList NTLV list to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_NTLVList(
    json_object * parent,
    const char * name,
    List_t *p_pList
);

/**
 * Get an NTLV list from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pList Destination for the NTLV list.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_NTLVList(
    json_object * parent,
    const char * name,
    List_t **p_pList
);

/**
 * Add a hash value to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pHash Hash value to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_Hash(
    json_object * parent,
    const char * name,
    const struct Hash *p_pHash
);

/**
 * Get a hash value from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pHash Destination for the hash value.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_Hash(
    json_object * parent,
    const char *name,
    struct Hash **p_pHash
);

/**
 * Add a block identifier to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pId Block identifier to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_BlockId(
    json_object * parent,
    const char *name,
    struct BlockId *p_pId
);

/**
 * Get a block identifier from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pId Destination for the block identifier.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_BlockId(
    json_object * parent,
    const char * name,
    struct BlockId **p_pId
);

/**
 * Add a block to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pBlock Block to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_Block(
    json_object * parent,
    const char * name,
    struct Block *p_pBlock
);

/**
 * Get a block from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pBlock Destination for the block.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_Block(
    json_object * parent,
    const char * name,
    struct Block **p_pBlock
);

/**
 * Add an event to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pEvent Event to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_Event(
    json_object * parent,
    const char * name,
    struct Event *p_pEvent
);

/**
 * Get an event from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pEvent Destination for the event.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_Event(
    json_object * parent,
    const char * name,
    struct Event **p_pEvent
);

/**
 * Add an event identifier to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pEventId Event identifier to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_EventId(
    json_object * parent,
    const char * name,
    struct EventId *p_pEventId
);

/**
 * Get an event identifier from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pEventId Destination for the event identifier.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_EventId(
    json_object * parent,
    const char * name,
    struct EventId **p_pEventId
);

/**
 * Add a judgment to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pJudgment Judgment to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_Judgment(
    json_object * parent,
    const char * name,
    struct Judgment *p_pJudgment
);

/**
 * Get a judgment from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param p_pJudgment Destination for the judgment.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_Judgment(
    json_object * parent,
    const char * name,
    struct Judgment **p_pJudgment
);

/**
 * Add a nugget to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param nugget Nugget to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_Nugget(
    json_object * parent,
    const char * name,
    struct Nugget * nugget
);

/**
 * Get a nugget from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param r_nugget Destination for the nugget.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_Nugget(
    json_object * parent,
    const char * name,
    struct Nugget ** r_nugget
);

/**
 * Add a UUID list to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param list UUID list to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_UUIDList(
    json_object * parent,
    const char * name,
    List_t * list
);

/**
 * Get a UUID list from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param r_list Destination for the UUID list.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_UUIDList(
    json_object * parent,
    const char * name,
    List_t ** r_list
);

/**
 * Add a string list to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param list String list to store.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_StringList(
    json_object * parent,
    const char * name,
    List_t * list
);

/**
 * Get a string list from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param r_list Destination for the string list.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_StringList(
    json_object * parent,
    const char * name,
    List_t ** r_list
);

/**
 * Add an 8-bit unsigned integer list to a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param list 8-bit unsigned integer list to store.
 * @param count Number of list elements.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Put_uint8List(
    json_object * parent,
    const char * name,
    uint8_t *list,
    uint32_t count
);

/**
 * Get an 8-bit unsigned integer list from a JSON object.
 * @param parent Parent object.
 * @param name Name string.
 * @param list Destination for the 8-bit unsigned integer list.
 * @param count Destination for the number of list elements.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool JsonBuffer_Get_uint8List(
    json_object * parent,
    const char * name,
    uint8_t **list,
    uint32_t *count
);
#ifdef __cplusplus
}
#endif
#endif //RAZORBACK_JSON_BUFFER_H
