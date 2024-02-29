#include "config.h"
#include <razorback/debug.h>
#include <razorback/uuids.h>
#include <razorback/types.h>
#include <razorback/config_file.h>
#include <razorback/log.h>

#include "init.h"
#include <string.h>
static const char *sg_sDataTypesFile = ETC_DIR "/datatypes.conf";
static const char *sg_sIntelTypesFile = ETC_DIR "/inteltypes.conf";
//static const char *sg_sNtlvTypesFile = ETC_DIR "/ntlv_types.conf";
//static const char *sg_sNtlvNamesFile = ETC_DIR "/ntlv_names.conf";
static const char *sg_sNuggetsFile = ETC_DIR "/nuggets.conf";
static const char *sg_sNuggetTypesFile = ETC_DIR "/nuggettypes.conf";

// Libconfig handle
static config_t config;

static struct UUIDList *sg_DataTypeList;
static struct UUIDList *sg_IntelTypeList;
static struct UUIDList *sg_NtlvTypeList;
static struct UUIDList *sg_NtlvNameList;
static struct UUIDList *sg_NuggetList;
static struct UUIDList *sg_NuggetTypeList;

SO_PUBLIC bool
UUID_Add_List_Entry (struct UUIDList *p_pList, uuid_t p_uuid,
                   const char *p_sName, const char *p_sDescr)
{
    struct UUIDListNode *l_pListNode;
    pthread_mutex_lock(&p_pList->mutex);
    size_t l_iLen;
    if ((l_pListNode = calloc (1, sizeof (struct UUIDListNode))) == NULL)
    {
        pthread_mutex_unlock(&p_pList->mutex);
        return false;
    }

    l_pListNode->pNext = NULL;
    uuid_copy (l_pListNode->uuid, p_uuid);
    l_iLen = strlen (p_sName);
    if ((l_pListNode->sName = calloc (l_iLen + 1, sizeof (char))) == NULL)
    {
        free (l_pListNode);
        pthread_mutex_unlock(&p_pList->mutex);
        return false;
    }
    memcpy (l_pListNode->sName, p_sName, l_iLen + 1);
    l_iLen = strlen (p_sDescr);
    if ((l_pListNode->sDescription =
         calloc (l_iLen + 1, sizeof (char))) == NULL)
    {
        free (l_pListNode->sName);
        free (l_pListNode);
        pthread_mutex_unlock(&p_pList->mutex);
        return false;
    }
    memcpy (l_pListNode->sDescription, p_sDescr, l_iLen + 1);

    if (p_pList->pHead == NULL)
    {
        p_pList->pHead = l_pListNode;
        p_pList->pTail = l_pListNode;
    }
    else
    {
        p_pList->pTail->pNext = l_pListNode;
        p_pList->pTail = l_pListNode;
    }
    p_pList->count++;
    pthread_mutex_unlock(&p_pList->mutex);

    return true;
}

static bool
UUID_readUuidFile (const char *p_sFile, struct UUIDList *p_pList)
{
    config_setting_t *l_pUuidSetting, *l_pUuidSettingList;
    int l_iNumUuids, l_iCurrUuid = 0;
    const char *l_sName, *l_sDescr, *l_sUuid;
    uuid_t l_uuid;
    memset (&config, 0, sizeof (config));
    config_init (&config);
    if (config_read_file (&config, p_sFile) != CONFIG_TRUE)
    {
        rzb_log (LOG_ERR, "%s: %s", __func__, config_error_text (&config));
        config_destroy (&config);
        return false;
    }

    if ((l_pUuidSettingList = config_lookup (&config, "uuids")) == NULL)
    {
        rzb_log (LOG_ERR, "%s: %s", __func__, config_error_text (&config));
        config_destroy (&config);
        return false;
    }

    l_iNumUuids = config_setting_length (l_pUuidSettingList);
    for (l_iCurrUuid = 0; l_iCurrUuid < l_iNumUuids; l_iCurrUuid++)
    {
        if ((l_pUuidSetting =
             config_setting_get_elem (l_pUuidSettingList,
                                      l_iCurrUuid)) == NULL)
        {
            rzb_log (LOG_ERR, "%s: %s", __func__, config_error_text (&config));
            config_destroy (&config);
            return false;
        }

        config_setting_lookup_string (l_pUuidSetting, "name", &l_sName);
        config_setting_lookup_string (l_pUuidSetting, "uuid", &l_sUuid);
        config_setting_lookup_string (l_pUuidSetting, "description",
                                      &l_sDescr);
        if (uuid_parse (l_sUuid, l_uuid) == -1)
        {
            rzb_log (LOG_ERR, "%s: Failed to parse UUID: %s", __func__, l_sUuid);
            config_destroy (&config);
            return false;
        }
        if (!UUID_Add_List_Entry
            (p_pList, l_uuid, l_sName, l_sDescr))
        {
            rzb_log (LOG_ERR, "%s: Failed to insert UUID into list", __func__);
            config_destroy (&config);
            return false;
        }
        rzb_log (LOG_DEBUG, "%s: UUID Load - Name[%d]: %s", __func__, l_iCurrUuid, l_sName);
        rzb_log (LOG_DEBUG, "%s: UUID Load - UUID[%d]: %s", __func__, l_iCurrUuid, l_sUuid);
        rzb_log (LOG_DEBUG, "%s: UUID Load - Description[%d]: %s", __func__, l_iCurrUuid,
                 l_sDescr);

    }
    config_destroy (&config);
    return true;
}

SO_PUBLIC struct UUIDList *
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
    struct UUIDListNode *l_pListNode = NULL;
    struct UUIDList *l_pList = NULL;
    l_pList = UUID_Get_List(p_iType); 
    l_pListNode = l_pList->pHead;
    while (l_pListNode != NULL)
    {
        if (strcmp (l_pListNode->sName, p_sName) == 0)
            break;
        l_pListNode = l_pListNode->pNext;
    }
    return l_pListNode;
}


static struct UUIDListNode *
UUID_getNodeByUUID (uuid_t p_uuid, int p_iType)
{
    struct UUIDListNode *l_pListNode = NULL;
    struct UUIDList *l_pList = NULL;
    l_pList = UUID_Get_List(p_iType);
    l_pListNode = l_pList->pHead;
    while (l_pListNode != NULL)
    {
        if (uuid_compare (l_pListNode->uuid, p_uuid) == 0)
            break;
        l_pListNode = l_pListNode->pNext;
    }

    return l_pListNode;
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
    UUID_readUuidFile (sg_sDataTypesFile, sg_DataTypeList);
    UUID_readUuidFile (sg_sIntelTypesFile, sg_IntelTypeList);
    UUID_readUuidFile (sg_sNuggetsFile, sg_NuggetList);
    UUID_readUuidFile (sg_sNuggetTypesFile, sg_NuggetTypeList);

}

SO_PUBLIC bool
UUID_Get_UUID (const char *p_sName, int p_iType, uuid_t r_uuid)
{
    struct UUIDList *l_pList;
    struct UUIDListNode *l_pListNode;

    l_pList = UUID_Get_List(p_iType);
    pthread_mutex_lock(&l_pList->mutex);
    if ((l_pListNode = UUID_getNodeByName (p_sName, p_iType)) == NULL)
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return false;
    }
    uuid_copy(r_uuid, l_pListNode->uuid);
    pthread_mutex_unlock(&l_pList->mutex);
    return true;
}

SO_PUBLIC char *
UUID_Get_Description (const char *p_sName, int p_iType)
{
    struct UUIDList *l_pList;
    struct UUIDListNode *l_pListNode;
    char * ret;
    l_pList = UUID_Get_List(p_iType);
    pthread_mutex_lock(&l_pList->mutex);

    if ((l_pListNode = UUID_getNodeByName (p_sName, p_iType)) == NULL)
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return NULL;
    }
    if (asprintf(&ret, "%s", l_pListNode->sDescription) == -1 )
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return NULL;
    }
    pthread_mutex_unlock(&l_pList->mutex);
    return ret;
}

SO_PUBLIC char *
UUID_Get_DescriptionByUUID (uuid_t p_uuid, int p_iType)
{
    struct UUIDList *l_pList;
    struct UUIDListNode *l_pListNode;
    char * ret;
    l_pList = UUID_Get_List(p_iType);
    pthread_mutex_lock(&l_pList->mutex);

    if ((l_pListNode = UUID_getNodeByUUID (p_uuid, p_iType)) == NULL)
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return NULL;
    }
    if (asprintf(&ret, "%s", l_pListNode->sDescription) == -1 )
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return NULL;
    }
    pthread_mutex_unlock(&l_pList->mutex);
    return ret;
}

SO_PUBLIC char *
UUID_Get_NameByUUID (uuid_t p_uuid, int p_iType)
{
    struct UUIDList *l_pList;
    struct UUIDListNode *l_pListNode;
    char * ret;
    l_pList = UUID_Get_List(p_iType);
    pthread_mutex_lock(&l_pList->mutex);

    if ((l_pListNode = UUID_getNodeByUUID (p_uuid, p_iType)) == NULL)
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return NULL;
    }
    if (asprintf(&ret, "%s", l_pListNode->sName) == -1 )
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return NULL;
    }
    pthread_mutex_unlock(&l_pList->mutex);
    return ret;
}


SO_PUBLIC char *
UUID_Get_UUIDAsString (const char *p_sName, int p_iType)
{
    struct UUIDList *l_pList;
    struct UUIDListNode *l_pListNode;
    char *l_sTemp;

    l_pList = UUID_Get_List(p_iType);
    pthread_mutex_lock(&l_pList->mutex);

    if ((l_pListNode = UUID_getNodeByName (p_sName, p_iType)) == NULL)
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return NULL;
    }
    if ((l_sTemp = calloc (UUID_STRING_LENGTH, sizeof (char))) == NULL)
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return NULL;
    }
    uuid_unparse (l_pListNode->uuid, l_sTemp);
    pthread_mutex_unlock(&l_pList->mutex);
    return l_sTemp;
}

SO_PUBLIC bool
UUID_Get_First_UUID (int p_iType, uuid_t r_uuid)
{
    struct UUIDList *l_pList;
    l_pList = UUID_Get_List(p_iType);

    pthread_mutex_lock(&l_pList->mutex);
    uuid_copy(r_uuid, l_pList->pHead->uuid);
    pthread_mutex_unlock(&l_pList->mutex);

    return true;
}

SO_PUBLIC bool
UUID_Get_Next_UUID (uuid_t p_uuid, int p_iType, uuid_t r_uuid)
{
    struct UUIDList *l_pList;
    struct UUIDListNode *l_pListNode;
    l_pList = UUID_Get_List(p_iType);
    pthread_mutex_lock(&l_pList->mutex);
    if ((l_pListNode = UUID_getNodeByUUID (p_uuid, p_iType)) == NULL)
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return false;
    }
    if (l_pListNode->pNext == NULL)
    {
        pthread_mutex_unlock(&l_pList->mutex);
        return false;
    }
    uuid_copy(r_uuid, l_pListNode->pNext->uuid);
    pthread_mutex_unlock(&l_pList->mutex);
    return true;
}

SO_PUBLIC struct UUIDList * 
UUID_Create_List (void)
{
    struct UUIDList *list;
    pthread_mutexattr_t mutexAttr;
    if (( list = calloc(1, sizeof(struct UUIDList))) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to allocate new list", __func__);
        return NULL;
    }
    memset(&mutexAttr, 0, sizeof(pthread_mutexattr_t));
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&list->mutex, &mutexAttr); 
    return list;
}

SO_PUBLIC void 
UUID_Clear_List(struct UUIDList *list)
{
    struct UUIDListNode *current, *next;
    pthread_mutex_lock(&list->mutex);
    current = list->pHead;
    while (current != NULL) 
    {
        next = current->pNext;
        if (current->sName != NULL)
            free(current->sName);
        if (current->sDescription != NULL)
            free(current->sDescription);
        free(current);
        current = next;
    }
    list->pHead = NULL;
    list->pTail = NULL;
    list->count = 0;
    pthread_mutex_unlock(&list->mutex);
}

