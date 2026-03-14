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
#include <razorback/uuids.h>
#include <razorback/types.h>
#include <razorback/config_file.h>
#include <razorback/log.h>
#include <razorback/list.h>

#include "init.h"
#include <string.h>

#ifdef _MSC_VER
#include "bobins.h"
#endif

#define CORRELATION_UUID "2fd75fa5-778b-443e-b910-1e19044e81e1"
#define CORRELATION_DESC "Correlation Nugget"
#define INTEL_UUID "356112d8-f4f1-41dc-b3f7-cace5674c2ec"
#define INTEL_DESC "Intel Nugget"
#define DEFENSE_UUID "5e9c1296-ad6a-423f-9eca-9f817c72c444"
#define DEFENSE_DESC "Defense Update Nugget"
#define OUTPUT_UUID "a3d0d1f9-c049-474e-bf01-2128ea00a751"
#define OUTPUT_DESC "Output Nugget"
#define COLLECTION_UUID "c38b113a-27fd-417c-b9fa-f3aa0af5cb53"
#define COLLECTION_DESC "Data Collector Nugget"
#define INSPECTION_UUID "d95aee72-9186-4236-bf23-8ff77dac630b"
#define INSPECTION_DESC "Inspection Nugget"
#define DISPATCHER_UUID "1117de3c-6fe8-4291-84ae-36cdf2f91017"
#define DISPATCHER_DESC "Message Dispatcher Nugget"
#define MASTER_UUID "ca51afd1-41b8-4c6b-b221-9faef0d202a7"
#define MASTER_DESC "Master Nugget"


#define UUID_KEY_NAME 0
#define UUID_KEY_UUID 1
struct UUIDKey
{
    int type;
    uuid_t uuid;
    const char *name;
};
static List_t *sg_DataTypeList;
static List_t *sg_IntelTypeList;
static List_t *sg_NtlvTypeList;
static List_t *sg_NtlvNameList;
static List_t *sg_NuggetList;
static List_t *sg_NuggetTypeList;

SO_PUBLIC bool
UUID_Add_List_Entry (List_t *list, uuid_t p_uuid,
                   const char *p_sName, const char *p_sDescr)
{
    struct UUIDListNode *l_pListNode;
    size_t l_iLen;
    if ((l_pListNode = calloc (1, sizeof (struct UUIDListNode))) == NULL)
    {
        return false;
    }

    uuid_copy (l_pListNode->uuid, p_uuid);
    l_iLen = strlen (p_sName);
    if ((l_pListNode->sName = calloc (l_iLen + 1, sizeof (char))) == NULL)
    {
        free (l_pListNode);
        return false;
    }
    memcpy (l_pListNode->sName, p_sName, l_iLen + 1);
    l_iLen = strlen (p_sDescr);
    if ((l_pListNode->sDescription =
         calloc (l_iLen + 1, sizeof (char))) == NULL)
    {
        free (l_pListNode->sName);
        free (l_pListNode);
        return false;
    }
    memcpy (l_pListNode->sDescription, p_sDescr, l_iLen + 1);

    return List_Push(list, l_pListNode);
}

SO_PUBLIC List_t *
UUID_Get_List(int type)
{
    switch (type)
    {
    case UUID_TYPE_DATA_TYPE:
        return sg_DataTypeList;
    case UUID_TYPE_INTEL_TYPE:
        return sg_IntelTypeList;
    case UUID_TYPE_NTLV_TYPE:
        return sg_NtlvTypeList;
    case UUID_TYPE_NTLV_NAME:
        return sg_NtlvNameList;
    case UUID_TYPE_NUGGET:
        return sg_NuggetList;
    case UUID_TYPE_NUGGET_TYPE:
        return sg_NuggetTypeList;
    default:
        return NULL;
    }
}

static struct UUIDListNode *
UUID_getNodeByName (const char *p_sName, int p_iType)
{
    List_t *list = NULL;
    struct UUIDKey key;
    key.type=UUID_KEY_NAME;
    key.name=p_sName;
    list = UUID_Get_List(p_iType);

    return (struct UUIDListNode *)List_Find(list, &key);
}


static struct UUIDListNode *
UUID_getNodeByUUID (uuid_t p_uuid, int p_iType)
{
    List_t *list = NULL;
    struct UUIDKey key;
    key.type=UUID_KEY_UUID;
    uuid_copy(key.uuid, p_uuid);
    list = UUID_Get_List(p_iType);
    return (struct UUIDListNode *)List_Find(list, &key);
}

static void
init_NuggetTypes(void)
{
    uuid_t uuid;
    uuid_parse(DISPATCHER_UUID,uuid);
    UUID_Add_List_Entry(sg_NuggetTypeList, uuid, NUGGET_TYPE_DISPATCHER, DISPATCHER_DESC);
    uuid_parse(MASTER_UUID,uuid);
    UUID_Add_List_Entry(sg_NuggetTypeList, uuid, NUGGET_TYPE_MASTER, MASTER_DESC);
    uuid_parse(COLLECTION_UUID,uuid);
    UUID_Add_List_Entry(sg_NuggetTypeList, uuid, NUGGET_TYPE_COLLECTION, COLLECTION_DESC);
    uuid_parse(INSPECTION_UUID,uuid);
    UUID_Add_List_Entry(sg_NuggetTypeList, uuid, NUGGET_TYPE_INSPECTION, INSPECTION_DESC);
    uuid_parse(OUTPUT_UUID,uuid);
    UUID_Add_List_Entry(sg_NuggetTypeList, uuid, NUGGET_TYPE_OUTPUT, OUTPUT_DESC);
    uuid_parse(INTEL_UUID,uuid);
    UUID_Add_List_Entry(sg_NuggetTypeList, uuid, NUGGET_TYPE_INTEL, INTEL_DESC);
    uuid_parse(DEFENSE_UUID,uuid);
    UUID_Add_List_Entry(sg_NuggetTypeList, uuid, NUGGET_TYPE_DEFENSE, DEFENSE_DESC);
}
void
initUuids (void)
{
    sg_DataTypeList = UUID_Create_List();
    sg_IntelTypeList = UUID_Create_List();
    sg_NtlvTypeList = UUID_Create_List();
    sg_NtlvNameList = UUID_Create_List();
    sg_NuggetList = UUID_Create_List();
    sg_NuggetTypeList = UUID_Create_List();
    init_NuggetTypes();
}

SO_PUBLIC bool
UUID_Get_UUID (const char *p_sName, int p_iType, uuid_t r_uuid)
{
    struct UUIDListNode *l_pListNode;

    if ((l_pListNode = UUID_getNodeByName (p_sName, p_iType)) == NULL)
    {
        return false;
    }
    uuid_copy(r_uuid, l_pListNode->uuid);
    return true;
}

SO_PUBLIC char *
UUID_Get_Description (const char *p_sName, int p_iType)
{
    struct UUIDListNode *l_pListNode;
    char * ret;

    if ((l_pListNode = UUID_getNodeByName (p_sName, p_iType)) == NULL)
    {
        return NULL;
    }
    if (asprintf(&ret, "%s", l_pListNode->sDescription) == -1 )
    {
        return NULL;
    }
    return ret;
}

SO_PUBLIC char *
UUID_Get_DescriptionByUUID (uuid_t p_uuid, int p_iType)
{
    struct UUIDListNode *l_pListNode;
    char * ret;

    if ((l_pListNode = UUID_getNodeByUUID (p_uuid, p_iType)) == NULL)
    {
        return NULL;
    }
    if (asprintf(&ret, "%s", l_pListNode->sDescription) == -1 )
    {
        return NULL;
    }
    return ret;
}

SO_PUBLIC char *
UUID_Get_NameByUUID (uuid_t p_uuid, int p_iType)
{
    struct UUIDListNode *l_pListNode;
    char * ret;

    if ((l_pListNode = UUID_getNodeByUUID (p_uuid, p_iType)) == NULL)
    {
        return NULL;
    }
    if (asprintf(&ret, "%s", l_pListNode->sName) == -1 )
    {
        return NULL;
    }
    return ret;
}


SO_PUBLIC char *
UUID_Get_UUIDAsString (const char *p_sName, int p_iType)
{
    struct UUIDListNode *l_pListNode;
    char *l_sTemp;


    if ((l_pListNode = UUID_getNodeByName (p_sName, p_iType)) == NULL)
    {
        return NULL;
    }
    if ((l_sTemp = calloc (UUID_STRING_LENGTH, sizeof (char))) == NULL)
    {
        return NULL;
    }
    uuid_unparse (l_pListNode->uuid, l_sTemp);
    return l_sTemp;
}

static int
UUID_Cmp(void *a, void *b)
{
    return -1;
}

static int
UUID_KeyCmp(void *a, const void *id)
{
    struct UUIDListNode *current = (struct UUIDListNode *)a;
    const struct UUIDKey *key = (const struct UUIDKey *)id;
    switch (key->type)
    {
    case UUID_KEY_UUID:
        return uuid_compare(key->uuid, current->uuid);
    case UUID_KEY_NAME:
        return strcmp(key->name, current->sName);
    }

    return -1;
}
static void *
UUID_Clone(void *o)
{
    struct UUIDListNode *orig = o;
    struct UUIDListNode *new;
    if ((new =calloc(1,sizeof(struct UUIDListNode))) == NULL)
        return NULL;
    uuid_copy(new->uuid, orig->uuid);
    if (orig->sName != NULL)
    {
        if (asprintf(&new->sName, "%s", orig->sName) == -1)
        {
            free(new);
            return NULL;
        }
    }
    if (orig->sDescription != NULL)
    {
        if (asprintf(&new->sDescription, "%s", orig->sDescription) == -1)
        {
            free(new);
            return NULL;
        }
    }

    return new;
}

static void
UUID_Destroy(void *a)
{
    struct UUIDListNode *current = (struct UUIDListNode *)a;
    if (current->sName != NULL)
        free(current->sName);
    if (current->sDescription != NULL)
        free(current->sDescription);
    free(current);
}

SO_PUBLIC List_t *
UUID_Create_List (void)
{
    List_t *list;
    list = List_Create(LIST_MODE_GENERIC,
            UUID_Cmp,
            UUID_KeyCmp,
            UUID_Destroy,
            UUID_Clone, NULL, NULL);

    if (list == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_CORE, "%s: Failed to allocate new list", __func__);
        return NULL;
    }
    return list;
}

static int
UUID_MessageAddNameSize(void *vItem, void *vCount)
{
    struct UUIDListNode *item = (struct UUIDListNode *)vItem;
    size_t * count = (size_t*)vCount;
    *count = *count + strlen(item->sName) +1;
    return LIST_EACH_OK;
}

size_t
UUIDList_BinarySize(List_t *list)
{
    size_t size = List_Length(list)*16;
    List_ForEach(list, UUID_MessageAddNameSize, &size);
    return size;
}
