/** JSON Buffer Implimentation 
 * @file binary_buffer.h
 */
#ifndef RAZORBACK_JSON_BUFFER_H
#define RAZORBACK_JSON_BUFFER_H

#include <razorback/types.h>
#include <razorback/messages.h>
#include <razorback/ntlv.h>
#include <json/json.h>



extern bool JsonBuffer_Put_uint8_t (json_object * parent,
                                    const char *name, uint8_t p_iValue);

extern bool JsonBuffer_Put_uint16_t (json_object * parent, const char * name, uint16_t p_iValue);

extern bool JsonBuffer_Put_uint32_t (json_object * parent, const char * name, uint32_t p_iValue);

extern bool JsonBuffer_Put_uint64_t (json_object * parent, const char * name, uint64_t p_iValue);

extern bool JsonBuffer_Put_ByteArray (json_object * parent, const char *name, 
                                      uint32_t p_iSize,
                                      const uint8_t * p_pByteArray);

extern bool JsonBuffer_Put_String (json_object * parent, const char * name,
                                   const char * p_sString);

extern bool JsonBuffer_Get_uint8_t (json_object * parent, const char * name, uint8_t * p_pValue);

extern bool JsonBuffer_Get_uint16_t (json_object * parent, const char * name,
                                     uint16_t * p_pValue);

extern bool JsonBuffer_Get_uint32_t (json_object * parent, const char *name,
                                     uint32_t * p_pValue);

extern bool JsonBuffer_Get_uint64_t (json_object * parent, const char *name,
                                     uint64_t * p_pValue);

extern char *JsonBuffer_Get_String (json_object * parent, const char * name);

extern bool JsonBuffer_Get_ByteArray (json_object * parent, const char * name,
                                      uint32_t * p_iSize,
                                      uint8_t ** p_pByteArray);

extern bool JsonBuffer_Get_UUID (json_object * parent, const char *name, uuid_t p_uuid);
extern bool JsonBuffer_Put_UUID (json_object * parent, const char * name, uuid_t p_uuid);

extern bool JsonBuffer_Put_NTLVList (json_object * parent, const char * name,
                                     struct List *p_pList);

extern bool JsonBuffer_Get_NTLVList (json_object * parent, const char * name,
                                     struct List **p_pList);

extern bool JsonBuffer_Put_Hash (json_object * parent, const char * name,
                                 const struct Hash *p_pHash);

extern bool JsonBuffer_Get_Hash (json_object * parent, const char *name, struct Hash **p_pHash);

extern bool JsonBuffer_Put_BlockId (json_object * parent, const char *name,
                                    struct BlockId *p_pId);


extern bool JsonBuffer_Get_BlockId (json_object * parent, const char * name,
                                    struct BlockId **p_pId);

extern bool JsonBuffer_Put_Block (json_object * parent, const char * name, 
                                  struct Block *p_pBlock);

extern bool JsonBuffer_Get_Block (json_object * parent, const char * name,
                                  struct Block **p_pBlock);

extern bool JsonBuffer_Put_Event (json_object * parent, const char * name, 
                                  struct Event *p_pEvent);

extern bool JsonBuffer_Get_Event (json_object * parent, const char * name,
                                  struct Event **p_pEvent);


extern bool JsonBuffer_Put_EventId (json_object * parent, const char * name,
                                    struct EventId *p_pEventId);

extern bool JsonBuffer_Get_EventId (json_object * parent, const char * name, 
                                    struct EventId **p_pEventId);

extern bool JsonBuffer_Put_Judgment (json_object * parent, const char * name,
                                     struct Judgment *p_pJudgment);

extern bool JsonBuffer_Get_Judgment (json_object * parent, const char * name, 
                                     struct Judgment **p_pJudgment);

bool JsonBuffer_Put_Nugget (json_object * parent, const char * name, 
                                           struct Nugget * nugget);

bool JsonBuffer_Get_Nugget (json_object * parent, const char * name, 
                                           struct Nugget ** r_nugget);

bool JsonBuffer_Put_UUIDList (json_object * parent, const char * name, 
                                           struct List * list);
bool JsonBuffer_Get_UUIDList (json_object * parent, const char * name, 
                                           struct List ** r_list);

bool JsonBuffer_Put_StringList (json_object * parent, const char * name, 
                                           struct List * list);
bool JsonBuffer_Get_StringList (json_object * parent, const char * name, 
                                           struct List ** r_list);
bool 
JsonBuffer_Put_uint8List (json_object * parent, const char * name, 
                                           uint8_t *list, uint32_t count);
bool 
JsonBuffer_Get_uint8List (json_object * parent, const char * name, 
                                           uint8_t **list, uint32_t *count);

#endif //RAZORBACK_JSON_BUFFER_H
