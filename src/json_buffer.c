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
#include <razorback/hash.h>
#include <razorback/list.h>
#include <razorback/uuids.h>
#include <razorback/block_id.h>
#include <razorback/block.h>
#include <razorback/event.h>
#include <razorback/judgment.h>
#include <razorback/string_list.h>
#include <razorback/log.h>
#include <razorback/json_buffer.h>
#include <razorback/nugget.h>
#ifdef _MSC_VER
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include "bobins.h"
#include <stdio.h>
#define NUM_FMT "%I64u"
#else //_MSC_VER
#include <arpa/inet.h>
#include <sys/socket.h>
#define NUM_FMT "%ju"
#endif //_MSC_VER


#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <errno.h>
#include <string.h>

static const char *
JsonBuffer_FieldName(const char *name)
{
    return (name != NULL) ? name : "<null>";
}

static const char *
JsonBuffer_TypeName(json_type type)
{
    switch (type) {
    case json_type_null:
        return "null";
    case json_type_boolean:
        return "boolean";
    case json_type_double:
        return "double";
    case json_type_int:
        return "integer";
    case json_type_object:
        return "object";
    case json_type_array:
        return "array";
    case json_type_string:
        return "string";
    default:
        return "unknown";
    }
}

static void
JsonBuffer_LogSerializationError(const char *func, const char *field,
                                 const char *detail)
{
    rzb_log(LOG_ERR, LOG_C_JSON,
            "%s: Failed to serialize field '%s': %s",
            func, JsonBuffer_FieldName(field), detail);
}

static void
JsonBuffer_LogDeserializationError(const char *func, const char *field,
                                   const char *detail)
{
    rzb_log(LOG_ERR, LOG_C_JSON,
            "%s: Failed to deserialize field '%s': %s",
            func, JsonBuffer_FieldName(field), detail);
}

static void
JsonBuffer_LogTypeMismatch(const char *func, const char *field,
                           const char *expected, json_object *object,
                           bool serializing)
{
    rzb_log(LOG_ERR, LOG_C_JSON,
            "%s: Failed to %s field '%s': expected %s, got %s",
            func,
            serializing ? "serialize" : "deserialize",
            JsonBuffer_FieldName(field),
            expected,
            (object == NULL) ? "missing" : JsonBuffer_TypeName(json_object_get_type(object)));
}

#define JSONBUFFER_RETURN_PUT(field, detail, expr) \
    do { \
        if (!(expr)) { \
            JsonBuffer_LogSerializationError(__func__, field, detail); \
            return false; \
        } \
    } while (0)

#define JSONBUFFER_RETURN_GET(field, detail, expr) \
    do { \
        if (!(expr)) { \
            JsonBuffer_LogDeserializationError(__func__, field, detail); \
            return false; \
        } \
    } while (0)

#define JSONBUFFER_GOTO_GET(field, detail, expr) \
    do { \
        if (!(expr)) { \
            JsonBuffer_LogDeserializationError(__func__, field, detail); \
            goto cleanup; \
        } \
    } while (0)

#define JSONBUFFER_RETURN_LIST_PUT(field, detail, expr) \
    do { \
        if (!(expr)) { \
            JsonBuffer_LogSerializationError(__func__, field, detail); \
            return LIST_EACH_ERROR; \
        } \
    } while (0)

static bool
JsonBuffer_Put_OptionalString(const char *caller, json_object *parent, const char *name,
                              const char *value, const char *detail)
{
    ASSERT(caller != NULL);
    ASSERT(parent != NULL);
    ASSERT(name != NULL);
    ASSERT(detail != NULL);
    if (caller == NULL || parent == NULL || name == NULL || detail == NULL) {
        JsonBuffer_LogSerializationError(__func__, name,
                                         "invalid optional string serialization parameters");
        return false;
    }
    if (value == NULL) {
        return true;
    }
    if (!JsonBuffer_Put_String(parent, name, value)) {
        JsonBuffer_LogSerializationError(caller, name, detail);
        return false;
    }
    return true;
}

static bool
JsonBuffer_Get_OptionalString(const char *caller, json_object *parent, const char *name,
                              char **value, const char *detail)
{
    ASSERT(caller != NULL);
    ASSERT(parent != NULL);
    ASSERT(name != NULL);
    ASSERT(value != NULL);
    ASSERT(detail != NULL);
    if (caller == NULL || parent == NULL || name == NULL || value == NULL || detail == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name,
                                           "invalid optional string deserialization parameters");
        return false;
    }

    *value = NULL;
    if (json_object_object_get(parent, name) == NULL) {
        return true;
    }
    if ((*value = JsonBuffer_Get_String(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(caller, name, detail);
        return false;
    }
    return true;
}

static bool
JsonBuffer_Put_JudgmentFields(const char *caller, json_object *parent,
                              struct Judgment *judgment)
{
    ASSERT(caller != NULL);
    ASSERT(parent != NULL);
    ASSERT(judgment != NULL);
    if (caller == NULL || parent == NULL || judgment == NULL) {
        JsonBuffer_LogSerializationError(__func__, "Judgment",
                                         "invalid judgment serialization parameters");
        return false;
    }

    if (!JsonBuffer_Put_UUID(parent, "Nugget_ID", judgment->uuidNuggetId)) {
        JsonBuffer_LogSerializationError(caller, "Nugget_ID",
                                         "failed to serialize judgment field");
        return false;
    }
    if (!JsonBuffer_Put_uint64_t(parent, "Seconds", judgment->iSeconds)) {
        JsonBuffer_LogSerializationError(caller, "Seconds",
                                         "failed to serialize judgment field");
        return false;
    }
    if (!JsonBuffer_Put_uint64_t(parent, "Nano_Seconds", judgment->iNanoSecs)) {
        JsonBuffer_LogSerializationError(caller, "Nano_Seconds",
                                         "failed to serialize judgment field");
        return false;
    }
    if (!JsonBuffer_Put_EventId(parent, "Event_ID", judgment->pEventId)) {
        JsonBuffer_LogSerializationError(caller, "Event_ID",
                                         "failed to serialize nested EventId");
        return false;
    }
    if (!JsonBuffer_Put_BlockId(parent, "Block_ID", judgment->pBlockId)) {
        JsonBuffer_LogSerializationError(caller, "Block_ID",
                                         "failed to serialize nested BlockId");
        return false;
    }
    if (!JsonBuffer_Put_uint8_t(parent, "Priority", judgment->iPriority)) {
        JsonBuffer_LogSerializationError(caller, "Priority",
                                         "failed to serialize judgment field");
        return false;
    }
    if (!JsonBuffer_Put_NTLVList(parent, "Metadata", judgment->pMetaDataList)) {
        JsonBuffer_LogSerializationError(caller, "Metadata",
                                         "failed to serialize nested NTLV list");
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(parent, "GID", judgment->iGID)) {
        JsonBuffer_LogSerializationError(caller, "GID",
                                         "failed to serialize judgment field");
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(parent, "SID", judgment->iSID)) {
        JsonBuffer_LogSerializationError(caller, "SID",
                                         "failed to serialize judgment field");
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(parent, "Set_SF_Flags", judgment->Set_SfFlags)) {
        JsonBuffer_LogSerializationError(caller, "Set_SF_Flags",
                                         "failed to serialize judgment field");
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(parent, "Set_Ent_Flags", judgment->Set_EntFlags)) {
        JsonBuffer_LogSerializationError(caller, "Set_Ent_Flags",
                                         "failed to serialize judgment field");
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(parent, "Unset_SF_Flags", judgment->Unset_SfFlags)) {
        JsonBuffer_LogSerializationError(caller, "Unset_SF_Flags",
                                         "failed to serialize judgment field");
        return false;
    }
    if (!JsonBuffer_Put_uint32_t(parent, "Unset_Ent_Flags", judgment->Unset_EntFlags)) {
        JsonBuffer_LogSerializationError(caller, "Unset_Ent_Flags",
                                         "failed to serialize judgment field");
        return false;
    }
    if (!JsonBuffer_Put_OptionalString(caller, parent, "Message",
                                       (const char *)judgment->sMessage,
                                       "failed to serialize judgment field")) {
        return false;
    }

    return true;
}

SO_PUBLIC bool JsonBuffer_Put_bool (json_object * parent,
                                        const char *name, bool p_iValue) {
    json_object *new;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }

    if ((new = json_object_new_boolean(p_iValue)) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate boolean value");
        return false;
    }

    json_object_object_add(parent, name, new);
    return true;
}

SO_PUBLIC bool JsonBuffer_Put_uint8_t (json_object * parent,
                                    const char *name, uint8_t p_iValue)
{
    JSONBUFFER_RETURN_PUT(name, "failed to serialize integer value",
                          JsonBuffer_Put_uint64_t(parent, name, p_iValue));
    return true;
}

SO_PUBLIC bool JsonBuffer_Put_uint16_t (json_object * parent, const char * name, uint16_t p_iValue)
{
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    JSONBUFFER_RETURN_PUT(name, "failed to serialize integer value",
                          JsonBuffer_Put_uint64_t(parent, name, p_iValue));
    return true;
}

SO_PUBLIC bool JsonBuffer_Put_uint32_t (json_object * parent, const char * name, uint32_t p_iValue)
{

    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    JSONBUFFER_RETURN_PUT(name, "failed to serialize integer value",
                          JsonBuffer_Put_uint64_t(parent, name, p_iValue));
    return true;
}

SO_PUBLIC bool JsonBuffer_Put_uint64_t (json_object * parent, const char * name, uint64_t p_iValue)
{
    json_object *new;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }



    if ((new = json_object_new_uint64(p_iValue)) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate integer value");
        return false;
    }

    json_object_object_add(parent, name, new);
    return true;
}

SO_PUBLIC bool JsonBuffer_Put_ByteArray (json_object * parent, const char *name,
                                      uint32_t p_iSize,
                                      const uint8_t * p_pByteArray)
{
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    char *buff;
    bool ret;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }

    b64 = BIO_new(BIO_f_base64());
    if (b64 == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate base64 BIO");
        return false;
    }
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    bmem = BIO_new(BIO_s_mem());
    if (bmem == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate memory BIO");
        BIO_free_all(b64);
        return false;
    }
    BIO_push(b64, bmem);
    BIO_write(b64, p_pByteArray, p_iSize);
    //BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    buff = (char *)malloc(bptr->length+1);
    if (buff == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate encoded byte buffer");
        BIO_free_all(b64);
        return false;
    }
    memcpy(buff, bptr->data, bptr->length);
    buff[bptr->length] = '\0';

    BIO_free_all(b64);

    ret = JsonBuffer_Put_String(parent, name, buff);
    if (!ret) {
        JsonBuffer_LogSerializationError(__func__, name,
                                         "failed to serialize encoded byte array");
    }

    free(buff);

    return ret;
}

SO_PUBLIC bool JsonBuffer_Put_String (json_object * parent, const char * name,
                                   const char * p_sString)
{
    json_object *new;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }

    if ((new = json_object_new_string(p_sString)) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate string value");
        return false;
    }

    json_object_object_add(parent, name, new);

    return true;
}

SO_PUBLIC bool JsonBuffer_Get_bool (json_object * parent, const char * name, bool * p_pValue)
{
    bool tmp;
    const char *tmpS;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_object_get(parent, name))  == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }

    json_type type = json_object_get_type(object);
    if (type == json_type_boolean) {
        tmp = json_object_get_boolean(object);
    }  else if (type == json_type_string) {
        tmpS = json_object_get_string(object);
        if (strcmp(tmpS, "true") == 0) {
            tmp = true;
        } else if (strcmp(tmpS, "false") == 0) {
            tmp = false;
        } else {
            JsonBuffer_LogDeserializationError(__func__, name, "invalid boolean string");
            return false;
        }
    } else {
        JsonBuffer_LogTypeMismatch(__func__, name, "boolean or string", object, false);
        return false;
    }

    *p_pValue = (bool) tmp;
    return true;
}

SO_PUBLIC bool JsonBuffer_Get_uint8_t (json_object * parent, const char * name, uint8_t * p_pValue)
{
    int tmp;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_object_get(parent, name))  == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_int) {
        JsonBuffer_LogTypeMismatch(__func__, name, "integer", object, false);
        return false;
    }

    tmp = json_object_get_int(object);
    *p_pValue = (uint8_t) tmp;
    return true;
}

SO_PUBLIC bool JsonBuffer_Get_uint16_t (json_object * parent, const char * name,
                                     uint16_t * p_pValue)
{
    int tmp;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_object_get(parent, name))  == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_int) {
        JsonBuffer_LogTypeMismatch(__func__, name, "integer", object, false);
        return false;
    }

    tmp = json_object_get_int(object);
    *p_pValue = (uint16_t) tmp;

    return true;
}

SO_PUBLIC bool JsonBuffer_Get_uint32_t (json_object * parent, const char *name,
                                     uint32_t * p_pValue)
{
    uint64_t val;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    JSONBUFFER_RETURN_GET(name, "failed to deserialize integer value",
                          JsonBuffer_Get_uint64_t(parent, name, &val));

    // TODO Bounds checking
    *p_pValue = (uint32_t)val;
    return true;
}

SO_PUBLIC bool JsonBuffer_Get_uint64_t (json_object * parent, const char *name,
                                     uint64_t * p_pValue)
{
    const char *tmp;
    uint64_t val;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_object_get(parent, name))  == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }

    json_type type = json_object_get_type(object);
    if (type == json_type_string) {
        tmp = json_object_get_string(object);
        if (sscanf(tmp,NUM_FMT, &val) != 1) {
            JsonBuffer_LogDeserializationError(__func__, name, "invalid integer string");
            return false;
        }
    } else if (type == json_type_int) {
        errno = 0;
        val = json_object_get_uint64(object);
        if (errno != 0) {
            JsonBuffer_LogDeserializationError(__func__, name, "failed to convert integer value");
            return false;
        }
    } else {
        JsonBuffer_LogTypeMismatch(__func__, name, "integer or string", object, false);
        return false;
    }

    *p_pValue = val;

    return true;
}

SO_PUBLIC char * JsonBuffer_Get_String (json_object * parent, const char * name)
{
    const char *tmp;
    char * ret;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return NULL;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return NULL;
    }
    if ((object = json_object_object_get(parent, name))  == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return NULL;
    }
    if (json_object_get_type(object) != json_type_string) {
        JsonBuffer_LogTypeMismatch(__func__, name, "string", object, false);
        return NULL;
    }
    tmp = json_object_get_string(object);
    if (asprintf(&ret, "%s", tmp) == -1) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate string copy");
        return NULL;
    }
    return ret;
}

SO_PUBLIC bool JsonBuffer_Get_ByteArray (json_object * parent, const char * name,
                                      uint32_t *p_iSize,
                                      uint8_t **p_pByteArray)
{
    json_object *object;
    const char *input;
    uint8_t *output;
    BIO *bmem, *b64;
    int inputLength;
    size_t outputLength;
    int decodedLength;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_string) {
        JsonBuffer_LogTypeMismatch(__func__, name, "string", object, false);
        return false;
    }
    input = json_object_get_string(object);
    inputLength = json_object_get_string_len(object);
    outputLength = (inputLength > 0) ? (size_t)inputLength : 1U;
    if ((output = calloc(outputLength, sizeof(uint8_t))) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate byte array");
        return false;
    }

    b64 = BIO_new (BIO_f_base64());
    if (b64 == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate base64 BIO");
        free(output);
        return false;
    }
    BIO_set_flags (b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new_mem_buf (input, inputLength);
    if (bmem == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate memory BIO");
        BIO_free_all(b64);
        free(output);
        return false;
    }
    bmem = BIO_push (b64, bmem);
    decodedLength = BIO_read (bmem, output, inputLength);
    if (decodedLength < 0) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to decode byte array");
        BIO_free_all (bmem);
        free(output);
        return false;
    }
    BIO_free_all (bmem);
    *p_iSize = (uint32_t)decodedLength;
    *p_pByteArray = output;
    return true;
}

SO_PUBLIC bool JsonBuffer_Get_UUID (json_object * parent, const char *name, uuid_t p_uuid)
{
    json_object *object;
    char *tmp;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    // get the container
    if ((object = json_object_object_get(parent, name))  == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_object) {
        JsonBuffer_LogTypeMismatch(__func__, name, "object", object, false);
        return false;
    }
    JSONBUFFER_RETURN_GET("id", "failed to deserialize UUID string",
                          (tmp = (char *)JsonBuffer_Get_String(object, "id")) != NULL);

    uuid_parse(tmp, p_uuid);
    free(tmp);
    return true;
}

SO_PUBLIC bool JsonBuffer_Put_UUID (json_object * parent, const char * name, uuid_t p_uuid)
{
    char uuid[UUID_STRING_LENGTH];
    json_object *new;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((new = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate object value");
        return false;
    }

    json_object_object_add(parent, name, new);
    uuid_unparse(p_uuid, uuid);

    JSONBUFFER_RETURN_PUT("id", "failed to serialize UUID string",
                          JsonBuffer_Put_String(new, "id", uuid));
    // TODO: Add other UUID attributes

    return true;
}

static bool
JsonBuffer_Get_NTLVItem (List_t *list, json_object *parent )
{
    char *str = NULL;
    uint8_t *byteData = NULL;
    uint32_t size = 0;
    size_t strLength = 0;
    bool success = false;
    uuid_t name, type, uuid;
    uint16_t port =0;
    uint8_t proto =0;
    uint8_t ipAddress[16];

    JSONBUFFER_RETURN_GET("Name", "failed to deserialize NTLV name",
                          JsonBuffer_Get_UUID(parent, "Name", name));

    JSONBUFFER_RETURN_GET("Type", "failed to deserialize NTLV type",
                          JsonBuffer_Get_UUID(parent, "Type", type));

    if (json_object_object_get(parent, "String_Value") != NULL) {
        JSONBUFFER_RETURN_GET("String_Value", "failed to deserialize string NTLV value",
                              (str = JsonBuffer_Get_String(parent, "String_Value")) != NULL);
    }

    if (json_object_object_get(parent, "Bin_Value") != NULL) {
        JSONBUFFER_RETURN_GET("Bin_Value", "failed to deserialize binary NTLV value",
                              JsonBuffer_Get_ByteArray(parent, "Bin_Value", &size, &byteData));
    }


    if (str != NULL) {
        strLength = strlen(str) + 1;
        UUID_Get_UUID(NTLV_TYPE_STRING, UUID_TYPE_NTLV_TYPE, uuid);
        if (uuid_compare(type, uuid) == 0) {
            if (!NTLVList_Add(list, name, type, (uint32_t)strLength,
                              (const uint8_t *)str)) {
                JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                                  "failed to append string NTLV value");
                goto cleanup;
            }
            success = true;
            goto cleanup;
        }

        UUID_Get_UUID(NTLV_TYPE_JSON, UUID_TYPE_NTLV_TYPE, uuid);
        if (uuid_compare(type, uuid) == 0) {
            if (!NTLVList_Add(list, name, type, (uint32_t)strLength,
                              (const uint8_t *)str)) {
                JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                                  "failed to append JSON NTLV value");
                goto cleanup;
            }
            success = true;
            goto cleanup;
        }

        UUID_Get_UUID(NTLV_TYPE_PORT, UUID_TYPE_NTLV_TYPE, uuid);
        if (uuid_compare(type, uuid) == 0) {
            if (sscanf(str, "%hu", &port) != 1) {
                JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                                  "invalid port string");
                goto cleanup;
            }
            if (!NTLVList_Add(list, name, type, sizeof(port),
                              (const uint8_t *)&port)) {
                JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                                  "failed to append port NTLV value");
                goto cleanup;
            }
            success = true;
            goto cleanup;
        }

        UUID_Get_UUID(NTLV_TYPE_IPPROTO, UUID_TYPE_NTLV_TYPE, uuid);
        if (uuid_compare(type, uuid) == 0) {
            if (sscanf(str, "%hhu", &proto) != 1) {
                JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                                  "invalid protocol string");
                goto cleanup;
            }
            if (!NTLVList_Add(list, name, type, sizeof(proto),
                              (const uint8_t *)&proto)) {
                JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                                  "failed to append protocol NTLV value");
                goto cleanup;
            }
            success = true;
            goto cleanup;
        }

        UUID_Get_UUID(NTLV_TYPE_IPv4_ADDR, UUID_TYPE_NTLV_TYPE, uuid);
        if (uuid_compare(type, uuid) == 0) {
            if (inet_pton(AF_INET, str, ipAddress) != 1) {
                JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                                  "invalid IPv4 address string");
                goto cleanup;
            }
            if (!NTLVList_Add(list, name, type, 4, ipAddress)) {
                JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                                  "failed to append IPv4 NTLV value");
                goto cleanup;
            }
            success = true;
            goto cleanup;
        }

        UUID_Get_UUID(NTLV_TYPE_IPv6_ADDR, UUID_TYPE_NTLV_TYPE, uuid);
        if (uuid_compare(type, uuid) == 0) {
            if (inet_pton(AF_INET6, str, ipAddress) != 1) {
                JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                                  "invalid IPv6 address string");
                goto cleanup;
            }
            if (!NTLVList_Add(list, name, type, 16, ipAddress)) {
                JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                                  "failed to append IPv6 NTLV value");
                goto cleanup;
            }
            success = true;
            goto cleanup;
        }

        JsonBuffer_LogDeserializationError(__func__, "Type",
                                          "unsupported NTLV type for string value");
        goto cleanup;
    } else if (byteData != NULL) {
        if (!NTLVList_Add(list, name, type, size, byteData)) {
            JsonBuffer_LogDeserializationError(__func__, "Bin_Value",
                                              "failed to append binary NTLV value");
            goto cleanup;
        }
        success = true;
    } else {
        JsonBuffer_LogDeserializationError(__func__, "String_Value",
                                          "no supported NTLV value representation was found");
        return false;
    }


cleanup:
    if (str != NULL)
        free(str);
    if (byteData != NULL)
        free(byteData);

    return success;
}

static int
JsonBuffer_Put_NTLVItem (struct NTLVItem *p_pItem, json_object *parent )
{
    json_object *object;
    char *str = NULL;
    bool doFree = false;
    uuid_t uuid;
    uint16_t port =0;
    uint8_t proto =0;

    if ((object = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, "<ntlv-item>",
                                         "failed to allocate object value");
        return false;
    }

    json_object_array_add(parent, object);

    JSONBUFFER_RETURN_LIST_PUT("Name", "failed to serialize NTLV name",
                               JsonBuffer_Put_UUID(object, "Name", p_pItem->uuidName));

    JSONBUFFER_RETURN_LIST_PUT("Type", "failed to serialize NTLV type",
                               JsonBuffer_Put_UUID(object, "Type", p_pItem->uuidType));

    UUID_Get_UUID(NTLV_TYPE_STRING, UUID_TYPE_NTLV_TYPE, uuid);
    if (uuid_compare(uuid, p_pItem->uuidType) == 0)
        str = (char *)p_pItem->pData;

    UUID_Get_UUID(NTLV_TYPE_PORT, UUID_TYPE_NTLV_TYPE, uuid);
    if (uuid_compare(uuid, p_pItem->uuidType) == 0)
    {
        memcpy(&port, p_pItem->pData, 2);

        if (asprintf(&str, "%hu", port) == -1) {
            JsonBuffer_LogSerializationError(__func__, "String_Value",
                                             "failed to format port value");
            return LIST_EACH_ERROR;
        }

        doFree = true;
        goto type_processed;
    }
    UUID_Get_UUID(NTLV_TYPE_IPPROTO, UUID_TYPE_NTLV_TYPE, uuid);
    if (uuid_compare(uuid, p_pItem->uuidType) == 0)
    {
        memcpy(&proto, p_pItem->pData, 1);

        if (asprintf(&str, "%hhu", port) == -1) {
            JsonBuffer_LogSerializationError(__func__, "String_Value",
                                             "failed to format protocol value");
            return LIST_EACH_ERROR;
        }

        doFree = true;
        goto type_processed;
    }
    UUID_Get_UUID(NTLV_TYPE_IPv4_ADDR, UUID_TYPE_NTLV_TYPE, uuid);
    if (uuid_compare(uuid, p_pItem->uuidType) == 0)
    {
        if ((str = calloc(1, INET_ADDRSTRLEN)) == NULL) {
            JsonBuffer_LogSerializationError(__func__, "String_Value",
                                             "failed to allocate IPv4 string buffer");
            return LIST_EACH_ERROR;
        }
        doFree=true;
        inet_ntop(AF_INET, p_pItem->pData, str, INET_ADDRSTRLEN);
        goto type_processed;
    }

    UUID_Get_UUID(NTLV_TYPE_IPv6_ADDR, UUID_TYPE_NTLV_TYPE, uuid);
    if (uuid_compare(uuid, p_pItem->uuidType) == 0)
    {
        if ((str = calloc(1, INET_ADDRSTRLEN)) == NULL) {
            JsonBuffer_LogSerializationError(__func__, "String_Value",
                                             "failed to allocate IPv6 string buffer");
            return LIST_EACH_ERROR;
        }
        doFree=true;
        inet_ntop(AF_INET6, p_pItem->pData, str, INET6_ADDRSTRLEN);
        goto type_processed;
    }

type_processed:
    if (str == NULL)
    {
        JSONBUFFER_RETURN_LIST_PUT("Bin_Value", "failed to serialize binary NTLV value",
                                   JsonBuffer_Put_ByteArray(object, "Bin_Value",
                                                            p_pItem->iLength, p_pItem->pData));
    }
    else
        if (!JsonBuffer_Put_String(object, "String_Value", str)) {
            free(str);
            JsonBuffer_LogSerializationError(__func__, "String_Value",
                                             "failed to serialize string NTLV value");
            return LIST_EACH_ERROR;
        }

    if (doFree)
        free(str);

    return LIST_EACH_OK;
}

SO_PUBLIC bool JsonBuffer_Put_NTLVList (json_object * parent, const char * name,
                                     List_t *p_pList)
{
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_array()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate array value");
        return false;
    }

    json_object_object_add(parent, name, object);

    if (!List_ForEach(p_pList, (int (*)(void*,void*))JsonBuffer_Put_NTLVItem, object)) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to serialize one or more NTLV items");
        return false;
    }

    return true;
}

SO_PUBLIC bool JsonBuffer_Get_NTLVList (json_object * parent, const char * name,
                                     List_t **p_pList)
{
    List_t *list;
    json_object *object;
    size_t i;

    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_array) {
        JsonBuffer_LogTypeMismatch(__func__, name, "array", object, false);
        return false;
    }
    parent = object;
    if ((list = NTLVList_Create()) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate NTLV list");
        return false;
    }

    //rzb_log(LOG_DEBUG, "%s: Processing NTLV list %s Length %zu", __func__, name, json_object_array_length(parent));
    for (i = 0; i < json_object_array_length(parent); i++)
    {
        //rzb_log(LOG_DEBUG, "%s: Processing NTLV item %zu", __func__, i);
        if (((object = json_object_array_get_idx(parent, i)) == NULL) ||
                (json_object_get_type(object) != json_type_object) )
        {
            JsonBuffer_LogDeserializationError(__func__, name,
                                              "encountered invalid NTLV list entry");
            List_Destroy(list);
            return false;
        }
        if (!JsonBuffer_Get_NTLVItem(list, object))
        {
            JsonBuffer_LogDeserializationError(__func__, name,
                                              "failed to deserialize one or more NTLV items");
            List_Destroy(list);
            return false;
        }
    }
    *p_pList = list;
    return true;
}

SO_PUBLIC bool JsonBuffer_Put_Hash (json_object * parent, const char * name,
                                 const struct Hash *p_pHash)
{
    json_object *object;
    const char *typeName = NULL;
    char *hashText = NULL;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate object value");
        return false;
    }
    json_object_object_add(parent, name, object);
    parent = object;
    switch (p_pHash->iType)
    {
    case HASH_TYPE_MD5:
        typeName = "MD5";
        break;
    case HASH_TYPE_SHA1:
        typeName = "SHA1";
        break;
    case HASH_TYPE_SHA224:
        typeName = "SHA224";
        break;
    case HASH_TYPE_SHA256:
        typeName = "SHA256";
        break;
    case HASH_TYPE_SHA512:
        typeName = "SHA512";
        break;
    default:
        JsonBuffer_LogSerializationError(__func__, name, "unsupported hash type");
        return false;
    }
    JSONBUFFER_RETURN_PUT("Type", "failed to serialize hash type",
                          JsonBuffer_Put_String(parent, "Type", typeName));
    if ((hashText = Hash_ToText(p_pHash)) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to render hash value");
        return false;
    }
    if (!JsonBuffer_Put_String(parent, "Value", hashText)) {
        JsonBuffer_LogSerializationError(__func__, "Value", "failed to serialize hash value");
        free(hashText);
        return false;
    }
    free(hashText);
    return true;
}

SO_PUBLIC bool JsonBuffer_Get_Hash (json_object * parent, const char *name, struct Hash **p_pHash)
{
    struct Hash *hash = NULL;
    json_object *object, *object2;
    const char *type, *data;
#ifdef _MSC_VER
    char tmp[3] = { '\0', '\0','\0' };
    unsigned long b;
#endif
    ASSERT( parent != NULL);
    ASSERT(name != NULL);

    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    // Get the container
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_object) {
        JsonBuffer_LogTypeMismatch(__func__, name, "object", object, false);
        return false;
    }
    parent = object;

    if ((object = json_object_object_get(parent, "Type")) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, "Type", "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_string) {
        JsonBuffer_LogTypeMismatch(__func__, "Type", "string", object, false);
        return false;
    }
    type = json_object_get_string(object);
    if ((object2 = json_object_object_get(parent, "Value")) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, "Value", "field not found");
        return false;
    }
    if (json_object_get_type(object2) != json_type_string) {
        JsonBuffer_LogTypeMismatch(__func__, "Value", "string", object2, false);
        return false;
    }
    data = json_object_get_string(object2);


    if (strcmp(type, "MD5") == 0)
        hash = Hash_Create_From_String(HASH_TYPE_MD5, data);
    else if (strcmp(type, "SHA1") == 0)
        hash = Hash_Create_From_String(HASH_TYPE_SHA1, data);
    else if (strcmp(type, "SHA224") == 0)
        hash = Hash_Create_From_String(HASH_TYPE_SHA224, data);
    else if (strcmp(type, "SHA256") == 0)
        hash = Hash_Create_From_String(HASH_TYPE_SHA256, data);
    else if (strcmp(type, "SHA512") == 0)
        hash = Hash_Create_From_String(HASH_TYPE_SHA512, data);

    if (hash == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to construct hash value");
        return false;
    }
    *p_pHash = hash;

    return true;
}

SO_PUBLIC bool JsonBuffer_Put_BlockId (json_object * parent, const char *name,
                                    struct BlockId *p_pId)
{
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate object value");
        return false;
    }
    json_object_object_add(parent, name, object);
    parent = object;
    JSONBUFFER_RETURN_PUT("Hash", "failed to serialize BlockId hash",
                          JsonBuffer_Put_Hash(parent, "Hash", p_pId->pHash));
    JSONBUFFER_RETURN_PUT("Size", "failed to serialize BlockId size",
                          JsonBuffer_Put_uint64_t(parent, "Size", p_pId->iLength));
    JSONBUFFER_RETURN_PUT("Data_Type", "failed to serialize BlockId data type",
                          JsonBuffer_Put_UUID(parent, "Data_Type", p_pId->uuidDataType));
    return true;
}


SO_PUBLIC bool JsonBuffer_Get_BlockId (json_object * parent, const char * name,
                                    struct BlockId **p_pId)
{
    struct BlockId *id = NULL;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    // Get the container
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_object) {
        JsonBuffer_LogTypeMismatch(__func__, name, "object", object, false);
        return false;
    }
    parent = object;
    if ((id = calloc(1, sizeof(struct BlockId))) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate BlockId");
        return false;
    }

    JSONBUFFER_GOTO_GET("Hash", "failed to deserialize nested BlockId hash",
                        JsonBuffer_Get_Hash(parent, "Hash", &id->pHash));
    JSONBUFFER_GOTO_GET("Size", "failed to deserialize BlockId size",
                        JsonBuffer_Get_uint64_t(parent, "Size", &id->iLength));
    JSONBUFFER_GOTO_GET("Data_Type", "failed to deserialize BlockId data type",
                        JsonBuffer_Get_UUID(parent, "Data_Type", id->uuidDataType));

    *p_pId = id;
    id = NULL;
    return true;

cleanup:
    if (id != NULL)
        BlockId_Destroy(id);
    return false;
}

SO_PUBLIC bool JsonBuffer_Put_Block (json_object * parent, const char * name,
                                  struct Block *p_pBlock)
{
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate object value");
        return false;
    }
    json_object_object_add(parent, name, object);
    parent = object;

    JSONBUFFER_RETURN_PUT("Id", "failed to serialize nested BlockId",
                          JsonBuffer_Put_BlockId(parent, "Id", p_pBlock->pId));

    if (p_pBlock->pParentId != NULL)
        JSONBUFFER_RETURN_PUT("Parent_Id", "failed to serialize nested BlockId",
                              JsonBuffer_Put_BlockId(parent, "Parent_Id", p_pBlock->pParentId));

    if (p_pBlock->pParentBlock != NULL)
        JSONBUFFER_RETURN_PUT("Parent", "failed to serialize nested Block",
                              JsonBuffer_Put_Block(parent, "Parent", p_pBlock->pParentBlock));

    if (p_pBlock->pMetaDataList != NULL)
        JSONBUFFER_RETURN_PUT("Metadata", "failed to serialize nested NTLV list",
                              JsonBuffer_Put_NTLVList(parent, "Metadata", p_pBlock->pMetaDataList));


    return true;
}

SO_PUBLIC bool JsonBuffer_Get_Block (json_object * parent, const char * name,
                                  struct Block **p_pBlock)
{
    struct Block *block = NULL;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    // Get the container
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_object) {
        JsonBuffer_LogTypeMismatch(__func__, name, "object", object, false);
        return false;
    }
    parent = object;
    if ((block = calloc(1, sizeof(struct Block))) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate Block");
        return false;
    }

    JSONBUFFER_GOTO_GET("Id", "failed to deserialize nested BlockId",
                        JsonBuffer_Get_BlockId(parent, "Id", &block->pId));

    if (json_object_object_get(parent, "Parent_Id") != NULL)
    {
        JSONBUFFER_GOTO_GET("Parent_Id", "failed to deserialize nested BlockId",
                            JsonBuffer_Get_BlockId(parent, "Parent_Id", &block->pParentId));
    }
    if (json_object_object_get(parent, "Parent") != NULL)
    {
        JSONBUFFER_GOTO_GET("Parent", "failed to deserialize nested Block",
                            JsonBuffer_Get_Block(parent, "Parent", &block->pParentBlock));
    }
    if (json_object_object_get(parent, "Metadata") != NULL) {
        JSONBUFFER_GOTO_GET("Metadata", "failed to deserialize nested NTLV list",
                            JsonBuffer_Get_NTLVList(parent, "Metadata", &block->pMetaDataList));
    } else {
        JsonBuffer_LogDeserializationError(__func__, "Metadata",
                                           "field not found in Block");
    }

    *p_pBlock = block;
    block = NULL;
    return true;

cleanup:
    if (block != NULL)
        Block_Destroy(block);
    return false;
}

SO_PUBLIC bool JsonBuffer_Put_Event (json_object * parent, const char * name,
                                  struct Event *p_pEvent)
{
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate object value");
        return false;
    }
    json_object_object_add(parent, name, object);
    parent = object;
    JSONBUFFER_RETURN_PUT("Id", "failed to serialize nested EventId",
                          JsonBuffer_Put_EventId(parent, "Id", p_pEvent->pId));

    if (p_pEvent->pParentId != NULL)
        JSONBUFFER_RETURN_PUT("Parent_Id", "failed to serialize nested EventId",
                              JsonBuffer_Put_EventId(parent, "Parent_Id", p_pEvent->pParentId));

    if (p_pEvent->pParent != NULL)
        JSONBUFFER_RETURN_PUT("Parent", "failed to serialize nested Event",
                              JsonBuffer_Put_Event(parent, "Parent", p_pEvent->pParent));

    if (p_pEvent->pMetaDataList != NULL)
        JSONBUFFER_RETURN_PUT("Metadata", "failed to serialize nested NTLV list",
                              JsonBuffer_Put_NTLVList(parent, "Metadata", p_pEvent->pMetaDataList));

    if (p_pEvent->pBlock != NULL)
        JSONBUFFER_RETURN_PUT("Block", "failed to serialize nested Block",
                              JsonBuffer_Put_Block(parent, "Block", p_pEvent->pBlock));
    return true;
}

SO_PUBLIC bool JsonBuffer_Get_Event (json_object * parent, const char * name,
                                  struct Event **p_pEvent)
{
    struct Event *event = NULL;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    // Get the container
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_object) {
        JsonBuffer_LogTypeMismatch(__func__, name, "object", object, false);
        return false;
    }
    parent = object;
    if ((event = calloc(1, sizeof(struct Event))) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate Event");
        return false;
    }
    JSONBUFFER_GOTO_GET("Id", "failed to deserialize nested EventId",
                        JsonBuffer_Get_EventId(parent, "Id", &event->pId));

    if (json_object_object_get(parent, "Parent_Id") != NULL)
    {
        JSONBUFFER_GOTO_GET("Parent_Id", "failed to deserialize nested EventId",
                            JsonBuffer_Get_EventId(parent, "Parent_Id", &event->pParentId));
    }

    if (json_object_object_get(parent, "Parent") != NULL)
    {
        JSONBUFFER_GOTO_GET("Parent", "failed to deserialize nested Event",
                            JsonBuffer_Get_Event(parent, "Parent", &event->pParent));
    }
    if (json_object_object_get(parent, "Metadata") != NULL)
    {
        JSONBUFFER_GOTO_GET("Metadata", "failed to deserialize nested NTLV list",
                            JsonBuffer_Get_NTLVList(parent, "Metadata", &event->pMetaDataList));
    }
    else
    {
        if ((event->pMetaDataList = NTLVList_Create()) == NULL)
            goto cleanup;

    }
    if (json_object_object_get(parent, "Block") != NULL)
    {
        JSONBUFFER_GOTO_GET("Block", "failed to deserialize nested Block",
                            JsonBuffer_Get_Block(parent, "Block", &event->pBlock));
    }

    *p_pEvent = event;
    event = NULL;
    return true;

cleanup:
    if (event != NULL)
        Event_Destroy(event);
    return false;
}


SO_PUBLIC bool JsonBuffer_Put_EventId (json_object * parent, const char * name,
                                    struct EventId *p_pEventId)
{
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate object value");
        return false;
    }
    json_object_object_add(parent, name, object);
    parent = object;
    // XXX: Refactor this!!
    if ((object = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, "Nugget", "failed to allocate object value");
        return false;
    }
    json_object_object_add(parent, "Nugget", object);
    JSONBUFFER_RETURN_PUT("Nugget.Id", "failed to serialize EventId nugget UUID",
                          JsonBuffer_Put_UUID(object, "Id", p_pEventId->uuidNuggetId));
    // XXX: End

    JSONBUFFER_RETURN_PUT("Seconds", "failed to serialize EventId seconds",
                          JsonBuffer_Put_uint64_t(parent, "Seconds", p_pEventId->iSeconds));

    JSONBUFFER_RETURN_PUT("Nano_Seconds", "failed to serialize EventId nanoseconds",
                          JsonBuffer_Put_uint64_t(parent, "Nano_Seconds", p_pEventId->iNanoSecs));

    return true;
}

SO_PUBLIC bool JsonBuffer_Get_EventId (json_object * parent, const char * name,
                                    struct EventId **p_pEventId)
{
    struct EventId *eventId = NULL;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }

    // Get the container
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_object) {
        JsonBuffer_LogTypeMismatch(__func__, name, "object", object, false);
        return false;
    }
    parent = object;
    // XXX: Refactor this!!
    if ((object = json_object_object_get(parent, "Nugget")) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, "Nugget", "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_object) {
        JsonBuffer_LogTypeMismatch(__func__, "Nugget", "object", object, false);
        return false;
    }
    // XXX: End

    if ((eventId = calloc(1, sizeof(struct EventId))) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate EventId");
        return false;
    }

    // XXX: Refactor this!!
    JSONBUFFER_GOTO_GET("Nugget.Id", "failed to deserialize EventId nugget UUID",
                        JsonBuffer_Get_UUID(object, "Id", eventId->uuidNuggetId));
    // XXX: End

    JSONBUFFER_GOTO_GET("Seconds", "failed to deserialize EventId seconds",
                        JsonBuffer_Get_uint64_t(parent, "Seconds", &eventId->iSeconds));

    JSONBUFFER_GOTO_GET("Nano_Seconds", "failed to deserialize EventId nanoseconds",
                        JsonBuffer_Get_uint64_t(parent, "Nano_Seconds", &eventId->iNanoSecs));
    *p_pEventId = eventId;
    eventId = NULL;
    return true;

cleanup:
    if (eventId != NULL)
        EventId_Destroy(eventId);
    return false;
}

SO_PUBLIC bool JsonBuffer_Put_Judgment (json_object * parent, const char * name,
                                     struct Judgment *p_pJudgment)
{
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate object value");
        return false;
    }
    json_object_object_add(parent, name, object);
    parent = object;
    return JsonBuffer_Put_JudgmentFields(__func__, parent, p_pJudgment);
}

SO_PUBLIC bool JsonBuffer_Get_Judgment (json_object * parent, const char * name,
                                     struct Judgment **p_pJudgment)
{
    struct Judgment *judgment = NULL;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    // Get the container
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_object) {
        JsonBuffer_LogTypeMismatch(__func__, name, "object", object, false);
        return false;
    }
    parent = object;
    if ((judgment = calloc(1, sizeof(struct Judgment))) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate Judgment");
        return false;
    }

    if (!JsonBuffer_Get_UUID(parent, "Nugget_ID", judgment->uuidNuggetId)) {
        JsonBuffer_LogDeserializationError(__func__, "Nugget_ID",
                                           "failed to deserialize judgment field");
        goto cleanup;
    }

    if (!JsonBuffer_Get_uint64_t(parent, "Seconds", &judgment->iSeconds)) {
        JsonBuffer_LogDeserializationError(__func__, "Seconds",
                                           "failed to deserialize judgment field");
        goto cleanup;
    }

    if (!JsonBuffer_Get_uint64_t(parent, "Nano_Seconds", &judgment->iNanoSecs)) {
        JsonBuffer_LogDeserializationError(__func__, "Nano_Seconds",
                                           "failed to deserialize judgment field");
        goto cleanup;
    }

    if (!JsonBuffer_Get_EventId(parent, "Event_ID", &judgment->pEventId)) {
        JsonBuffer_LogDeserializationError(__func__, "Event_ID",
                                           "failed to deserialize nested EventId");
        goto cleanup;
    }

    if (!JsonBuffer_Get_BlockId(parent, "Block_ID", &judgment->pBlockId)) {
        JsonBuffer_LogDeserializationError(__func__, "Block_ID",
                                           "failed to deserialize nested BlockId");
        goto cleanup;
    }

    if (!JsonBuffer_Get_uint8_t(parent, "Priority", &judgment->iPriority)) {
        JsonBuffer_LogDeserializationError(__func__, "Priority",
                                           "failed to deserialize judgment field");
        goto cleanup;
    }

    if (!JsonBuffer_Get_NTLVList(parent, "Metadata", &judgment->pMetaDataList)) {
        JsonBuffer_LogDeserializationError(__func__, "Metadata",
                                           "failed to deserialize nested NTLV list");
        goto cleanup;
    }

    if (!JsonBuffer_Get_uint32_t(parent, "GID", &judgment->iGID)) {
        JsonBuffer_LogDeserializationError(__func__, "GID",
                                           "failed to deserialize judgment field");
        goto cleanup;
    }

    if (!JsonBuffer_Get_uint32_t(parent, "SID", &judgment->iSID)) {
        JsonBuffer_LogDeserializationError(__func__, "SID",
                                           "failed to deserialize judgment field");
        goto cleanup;
    }

    if (!JsonBuffer_Get_uint32_t(parent, "Set_SF_Flags", &judgment->Set_SfFlags)) {
        JsonBuffer_LogDeserializationError(__func__, "Set_SF_Flags",
                                           "failed to deserialize judgment field");
        goto cleanup;
    }

    if (!JsonBuffer_Get_uint32_t(parent, "Set_Ent_Flags", &judgment->Set_EntFlags)) {
        JsonBuffer_LogDeserializationError(__func__, "Set_Ent_Flags",
                                           "failed to deserialize judgment field");
        goto cleanup;
    }

    if (!JsonBuffer_Get_uint32_t(parent, "Unset_SF_Flags", &judgment->Unset_SfFlags)) {
        JsonBuffer_LogDeserializationError(__func__, "Unset_SF_Flags",
                                           "failed to deserialize judgment field");
        goto cleanup;
    }

    if (!JsonBuffer_Get_uint32_t(parent, "Unset_Ent_Flags", &judgment->Unset_EntFlags)) {
        JsonBuffer_LogDeserializationError(__func__, "Unset_Ent_Flags",
                                           "failed to deserialize judgment field");
        goto cleanup;
    }

    {
        char *message = NULL;

        if (!JsonBuffer_Get_OptionalString(__func__, parent, "Message", &message,
                                           "failed to deserialize judgment field")) {
            goto cleanup;
        }
        judgment->sMessage = (uint8_t *)message;
    }
    *p_pJudgment = judgment;
    judgment = NULL;
    return true;

cleanup:
    if (judgment != NULL)
        Judgment_Destroy(judgment);
    return false;
}

SO_PUBLIC bool JsonBuffer_Put_Nugget (json_object * parent, const char * name,
                                           struct Nugget * nugget)
{
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate object value");
        return false;
    }
    json_object_object_add(parent, name, object);
    parent = object;

    JSONBUFFER_RETURN_PUT("Nugget_ID", "failed to serialize nugget field",
                          JsonBuffer_Put_UUID(parent, "Nugget_ID", nugget->uuidNuggetId));

    JSONBUFFER_RETURN_PUT("App_Type", "failed to serialize nugget field",
                          JsonBuffer_Put_UUID(parent, "App_Type", nugget->uuidApplicationType));

    JSONBUFFER_RETURN_PUT("Nugget_Type", "failed to serialize nugget field",
                          JsonBuffer_Put_UUID(parent, "Nugget_Type", nugget->uuidNuggetType));

    if (!JsonBuffer_Put_OptionalString(__func__, parent, "Name", nugget->sName,
                                       "failed to serialize nugget field")) {
        return false;
    }

    if (!JsonBuffer_Put_OptionalString(__func__, parent, "Location", nugget->sLocation,
                                       "failed to serialize nugget field")) {
        return false;
    }

    if (!JsonBuffer_Put_OptionalString(__func__, parent, "Contact", nugget->sContact,
                                       "failed to serialize nugget field")) {
        return false;
    }

    return true;
}

SO_PUBLIC bool JsonBuffer_Get_Nugget (json_object * parent, const char * name,
                                           struct Nugget ** r_nugget)
{
    struct Nugget *nugget = NULL;
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    // Get the container
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_object) {
        JsonBuffer_LogTypeMismatch(__func__, name, "object", object, false);
        return false;
    }
    parent = object;

    if ((nugget = calloc(1, sizeof(struct Nugget))) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate Nugget");
        return false;
    }

    if (!JsonBuffer_Get_UUID(parent, "Nugget_ID", nugget->uuidNuggetId)) {
        JsonBuffer_LogDeserializationError(__func__, "Nugget_ID",
                                           "failed to deserialize nugget field");
        goto cleanup;
    }

    if (!JsonBuffer_Get_UUID(parent, "App_Type", nugget->uuidApplicationType)) {
        JsonBuffer_LogDeserializationError(__func__, "App_Type",
                                           "failed to deserialize nugget field");
        goto cleanup;
    }
    if (!JsonBuffer_Get_UUID(parent, "Nugget_Type", nugget->uuidNuggetType)) {
        JsonBuffer_LogDeserializationError(__func__, "Nugget_Type",
                                           "failed to deserialize nugget field");
        goto cleanup;
    }
    if (!JsonBuffer_Get_OptionalString(__func__, parent, "Name", &nugget->sName,
                                       "failed to deserialize nugget field")) {
        goto cleanup;
    }
    if (!JsonBuffer_Get_OptionalString(__func__, parent, "Location", &nugget->sLocation,
                                       "failed to deserialize nugget field")) {
        goto cleanup;
    }

    if (!JsonBuffer_Get_OptionalString(__func__, parent, "Contact", &nugget->sContact,
                                       "failed to deserialize nugget field")) {
        goto cleanup;
    }
    *r_nugget=nugget;
    nugget = NULL;
    return true;

cleanup:
    if (nugget != NULL)
        Nugget_Destroy(nugget);
    return false;
}

static int
JsonBuffer_Put_UUIDList_Add(void *vnode, void *varray)
{
    struct UUIDListNode *node = vnode;
    char uuid[UUID_STRING_LENGTH];
    json_object *array = varray;
    json_object *object;
    if ((object = json_object_new_object()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, "<uuid-list-entry>",
                                         "failed to allocate object value");
        return LIST_EACH_ERROR;
    }

    uuid_unparse(node->uuid, uuid);
    JSONBUFFER_RETURN_LIST_PUT("id", "failed to serialize UUID list entry id",
                               JsonBuffer_Put_String(object, "id", uuid));
    if (node->sName != NULL)
        JSONBUFFER_RETURN_LIST_PUT("name", "failed to serialize UUID list entry name",
                                   JsonBuffer_Put_String(object, "name", node->sName));
    if (node->sDescription != NULL)
        JSONBUFFER_RETURN_LIST_PUT("description", "failed to serialize UUID list entry description",
                                   JsonBuffer_Put_String(object, "description", node->sDescription));

    json_object_array_add(array, object);

    return LIST_EACH_OK;
}

SO_PUBLIC bool
JsonBuffer_Put_UUIDList (json_object * parent, const char * name,
                                           List_t * list)
{
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_array()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate array value");
        return false;
    }
    json_object_object_add(parent, name, object);

    JSONBUFFER_RETURN_PUT(name, "failed to serialize UUID list entries",
                          List_ForEach(list, JsonBuffer_Put_UUIDList_Add, object));
    return true;
}

SO_PUBLIC bool JsonBuffer_Get_UUIDList (json_object * parent, const char * name,
                                           List_t ** r_list)
{
    List_t *list;
    json_object *object;
    uuid_t uuid;
    char *uuidS;
    char *nameS;
    char *desc;
    size_t i;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_array) {
        JsonBuffer_LogTypeMismatch(__func__, name, "array", object, false);
        return false;
    }
    parent = object;
    if ((list = UUID_Create_List()) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate UUID list");
        return false;
    }

    for (i = 0; i < json_object_array_length(parent); i++)
    {
        uuidS = NULL;
        nameS = NULL;
        desc = NULL;
        if (((object = json_object_array_get_idx(parent, i)) == NULL) ||
                (json_object_get_type(object) != json_type_object) )
        {
            JsonBuffer_LogDeserializationError(__func__, name,
                                              "encountered invalid UUID list entry");
            List_Destroy(list);
            return false;
        }
        if ((uuidS = JsonBuffer_Get_String(object, "id")) == NULL) {
            JsonBuffer_LogDeserializationError(__func__, "id",
                                              "failed to deserialize UUID list entry id");
            List_Destroy(list);
            return false;
        }
        if (!JsonBuffer_Get_OptionalString(__func__, object, "name", &nameS,
                                           "failed to deserialize UUID list entry name")) {
            free(uuidS);
            List_Destroy(list);
            return false;
        }
        if (!JsonBuffer_Get_OptionalString(__func__, object, "description", &desc,
                                           "failed to deserialize UUID list entry description")) {
            free(nameS);
            free(uuidS);
            List_Destroy(list);
            return false;
        }
        uuid_parse(uuidS, uuid);
        if (!UUID_Add_List_Entry(list, uuid, nameS, desc)) {
            JsonBuffer_LogDeserializationError(__func__, name,
                                              "failed to append UUID list entry");
            free(desc);
            free(nameS);
            free(uuidS);
            List_Destroy(list);
            return false;
        }
        free(nameS);
        free(uuidS);
        free(desc);
    }

    *r_list = list;
    return true;
}

static int
JsonBuffer_Put_StringList_Add(void *vnode, void *varray)
{
    char *node = vnode;
    json_object *array = varray;
    json_object *object;
    if ((object = json_object_new_string(node)) == NULL)
        return LIST_EACH_ERROR;

    json_object_array_add(array, object);

    return LIST_EACH_OK;
}

SO_PUBLIC bool
JsonBuffer_Put_StringList (json_object * parent, const char * name,
                                           List_t * list)
{
    json_object *object;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_array()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate array value");
        return false;
    }
    json_object_object_add(parent, name, object);

    List_ForEach(list, JsonBuffer_Put_StringList_Add, object);
    return true;
}

SO_PUBLIC bool
JsonBuffer_Get_StringList (json_object * parent, const char * name,
                                           List_t ** r_list)
{
    List_t *list;
    json_object *object;
    const char *string;
    size_t i;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_array) {
        JsonBuffer_LogTypeMismatch(__func__, name, "array", object, false);
        return false;
    }
    parent = object;
    if ((list = StringList_Create()) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate string list");
        return false;
    }
    for (i = 0; i < json_object_array_length(parent); i++)
    {
        if ((object = json_object_array_get_idx(parent, i)) == NULL)
        {
            JsonBuffer_LogDeserializationError(__func__, name,
                                              "encountered missing string list entry");
            List_Destroy(list);
            return false;
        }
        if (json_object_get_type(object) != json_type_string)
        {
            JsonBuffer_LogTypeMismatch(__func__, name, "string", object, false);
            List_Destroy(list);
            return false;
        }
        string = json_object_get_string(object);
        if (string == NULL)
        {
            JsonBuffer_LogDeserializationError(__func__, name,
                                              "failed to access string list entry");
            List_Destroy(list);
            return false;
        }
        if (!StringList_Add(list, string))
        {
            JsonBuffer_LogDeserializationError(__func__, name,
                                              "failed to append string list entry");
            List_Destroy(list);
            return false;
        }
    }

    *r_list = list;
    return true;
}


SO_PUBLIC bool
JsonBuffer_Put_uint8List (json_object * parent, const char * name,
                                           uint8_t *list, uint32_t count)
{
    json_object *object;
    uint32_t i;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_new_array()) == NULL) {
        JsonBuffer_LogSerializationError(__func__, name, "failed to allocate array value");
        return false;
    }
    json_object_object_add(parent, name, object);
    parent = object;
    for (i = 0; i < count; i++)
    {
        if ((object = json_object_new_int(list[i])) == NULL) {
            JsonBuffer_LogSerializationError(__func__, name,
                                             "failed to allocate uint8 list entry");
            return false;
        }

        json_object_array_add(parent, object);
    }
    return true;
}

SO_PUBLIC bool
JsonBuffer_Get_uint8List (json_object * parent, const char * name,
                                           uint8_t **list, uint32_t *count)
{
    json_object *object;
    uint32_t c =0;
    uint8_t *items = NULL;
    uint32_t i;
    ASSERT( parent != NULL);
    ASSERT(name != NULL);
    if (parent == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "parent object is NULL");
        return false;
    }
    if (name == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field name is NULL");
        return false;
    }
    if ((object = json_object_object_get(parent, name)) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "field not found");
        return false;
    }
    if (json_object_get_type(object) != json_type_array) {
        JsonBuffer_LogTypeMismatch(__func__, name, "array", object, false);
        return false;
    }
    parent = object;
    c = json_object_array_length(parent);
    if (c > 0 && (items = calloc(c, sizeof(uint8_t))) == NULL) {
        JsonBuffer_LogDeserializationError(__func__, name, "failed to allocate uint8 list");
        return false;
    }

    for ( i = 0; i < c; i++)
    {
        if (((object = json_object_array_get_idx(parent, i)) == NULL) ||
                (json_object_get_type(object) != json_type_int)) {
            JsonBuffer_LogDeserializationError(__func__, name,
                                              "encountered invalid uint8 list entry");
            free(items);
            return false;
        }
        items[i] = json_object_get_int(object);
    }

    *list = items;
    *count = c;
    return true;
}
