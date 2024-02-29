#include "config.h"
#include <razorback/debug.h>
#include <razorback/uuids.h>
#include <razorback/types.h>
#include <razorback/config_file.h>
#include <razorback/log.h>

#include <string.h>
static const char *sg_sDataTypesFile = ETC_DIR "/datatypes.conf";
static const char *sg_sIntelTypesFile = ETC_DIR "/inteltypes.conf";
static const char *sg_sNtlvTypesFile = ETC_DIR "/ntlv_types.conf";
static const char *sg_sNuggetsFile = ETC_DIR "/nuggets.conf";
static const char *sg_sNuggetTypesFile = ETC_DIR "/nuggettypes.conf";

// Libconfig handle
static config_t config;

struct UUIDListNode
{
    uuid_t uuid;
    char *sName;
    char *sDescription;
    conf_int_t iLength;
    struct UUIDListNode *pNext;
};

struct UUIDList
{
    uint32_t iType;
    struct UUIDListNode *pHead;
    struct UUIDListNode *pTail;

};

static struct UUIDList sg_DataTypeList = { UUID_TYPE_DATA_TYPE, NULL, NULL };
static struct UUIDList sg_IntelTypeList =
    { UUID_TYPE_INTEL_TYPE, NULL, NULL };
static struct UUIDList sg_NtlvTypeList = { UUID_TYPE_NTLV_TYPE, NULL, NULL };
static struct UUIDList sg_NuggetList = { UUID_TYPE_NUGGET, NULL, NULL };
static struct UUIDList sg_NuggetTypeList =
    { UUID_TYPE_NUGGET_TYPE, NULL, NULL };

static bool
UUID_addListEntry (struct UUIDList *p_pList, uuid_t p_uuid,
                   const char *p_sName, const char *p_sDescr,
                   conf_int_t p_iLength)
{
    struct UUIDListNode *l_pListNode;
    size_t l_iLen;
    if ((l_pListNode = calloc (1, sizeof (struct UUIDListNode))) == NULL)
    {
        return false;
    }

    l_pListNode->pNext = NULL;
    l_pListNode->iLength = p_iLength;
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

    return true;
}

static bool
UUID_readUuidFile (const char *p_sFile, struct UUIDList *p_pList)
{
    config_setting_t *l_pUuidSetting, *l_pUuidSettingList;
    int l_iNumUuids, l_iCurrUuid = 0;
    const char *l_sName, *l_sDescr, *l_sUuid;
    uuid_t l_uuid;
    conf_int_t l_iLength = 0;
    memset (&config, 0, sizeof (config));
    config_init (&config);
    if (config_read_file (&config, p_sFile) != CONFIG_TRUE)
    {
        rzb_log (LOG_ERR, "%s\n", config_error_text (&config));
        config_destroy (&config);
        return false;
    }

    if ((l_pUuidSettingList = config_lookup (&config, "uuids")) == NULL)
    {
        rzb_log (LOG_ERR, "%s\n", config_error_text (&config));
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
            rzb_log (LOG_ERR, "%s\n", config_error_text (&config));
            config_destroy (&config);
            return false;
        }

        config_setting_lookup_string (l_pUuidSetting, "name", &l_sName);
        config_setting_lookup_string (l_pUuidSetting, "uuid", &l_sUuid);
        config_setting_lookup_string (l_pUuidSetting, "description",
                                      &l_sDescr);
        if (uuid_parse (l_sUuid, l_uuid) == -1)
        {
            rzb_log (LOG_ERR, "Failed to parse UUID: %s\n", l_sUuid);
            config_destroy (&config);
            return false;
        }
        if (p_pList->iType == UUID_TYPE_NTLV_TYPE)
        {
            config_setting_lookup_int (l_pUuidSetting, "length", &l_iLength);
        }

        if (!UUID_addListEntry
            (p_pList, l_uuid, l_sName, l_sDescr, l_iLength))
        {
            rzb_log (LOG_ERR, "Failed to insert UUID into list\n");
            config_destroy (&config);
            return false;
        }
        rzb_log (LOG_DEBUG, "UUID Load - Name[%d]: %s", l_iCurrUuid, l_sName);
        rzb_log (LOG_DEBUG, "UUID Load - UUID[%d]: %s", l_iCurrUuid, l_sUuid);
        rzb_log (LOG_DEBUG, "UUID Load - Description[%d]: %s", l_iCurrUuid,
                 l_sDescr);
        rzb_log (LOG_DEBUG, "UUID Load - Length[%d]: %d", l_iCurrUuid,
                 (int) l_iLength);

    }
    config_destroy (&config);
    return true;
}

static struct UUIDListNode *
UUID_getNodeByName (const char *p_sName, int p_iType)
{
    struct UUIDListNode *l_pListNode = NULL;
    struct UUIDList *l_pList = NULL;
    switch (p_iType)
    {
    case UUID_TYPE_DATA_TYPE:
        l_pList = &sg_DataTypeList;
        break;
    case UUID_TYPE_INTEL_TYPE:
        l_pList = &sg_IntelTypeList;
        break;
    case UUID_TYPE_NTLV_TYPE:
        l_pList = &sg_NtlvTypeList;
        break;
    case UUID_TYPE_NUGGET:
        l_pList = &sg_NuggetList;
        break;
    case UUID_TYPE_NUGGET_TYPE:
        l_pList = &sg_NuggetTypeList;
        break;
    default:
        return NULL;
    }
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
    switch (p_iType)
    {
    case UUID_TYPE_DATA_TYPE:
        l_pList = &sg_DataTypeList;
        break;
    case UUID_TYPE_INTEL_TYPE:
        l_pList = &sg_IntelTypeList;
        break;
    case UUID_TYPE_NTLV_TYPE:
        l_pList = &sg_NtlvTypeList;
        break;
    case UUID_TYPE_NUGGET:
        l_pList = &sg_NuggetList;
        break;
    case UUID_TYPE_NUGGET_TYPE:
        l_pList = &sg_NuggetTypeList;
        break;
    default:
        return NULL;
    }
    l_pListNode = l_pList->pHead;
    while (l_pListNode != NULL)
    {
        if (uuid_compare (l_pListNode->uuid, p_uuid) == 0)
            break;
        l_pListNode = l_pListNode->pNext;
    }

    return l_pListNode;
}


SO_PUBLIC void __attribute__ ((constructor)) initUuids (void)
{
    UUID_readUuidFile (sg_sDataTypesFile, &sg_DataTypeList);
    UUID_readUuidFile (sg_sIntelTypesFile, &sg_IntelTypeList);
    UUID_readUuidFile (sg_sNtlvTypesFile, &sg_NtlvTypeList);
    UUID_readUuidFile (sg_sNuggetsFile, &sg_NuggetList);
    UUID_readUuidFile (sg_sNuggetTypesFile, &sg_NuggetTypeList);

}

SO_PUBLIC uuid_t *
UUID_Get_UUID (const char *p_sName, int p_iType)
{
    struct UUIDListNode *l_pListNode;
    if ((l_pListNode = UUID_getNodeByName (p_sName, p_iType)) == NULL)
        return NULL;
    return &l_pListNode->uuid;
}

SO_PUBLIC char *
UUID_Get_Description (const char *p_sName, int p_iType)
{
    struct UUIDListNode *l_pListNode;
    if ((l_pListNode = UUID_getNodeByName (p_sName, p_iType)) == NULL)
        return NULL;
    return l_pListNode->sDescription;
}

SO_PUBLIC char *
UUID_Get_DescriptionByUUID (uuid_t p_uuid, int p_iType)
{
    struct UUIDListNode *l_pListNode;
    if ((l_pListNode = UUID_getNodeByUUID (p_uuid, p_iType)) == NULL)
        return NULL;
    return l_pListNode->sDescription;
}
SO_PUBLIC char *
UUID_Get_NameByUUID (uuid_t p_uuid, int p_iType)
{
    struct UUIDListNode *l_pListNode;
    if ((l_pListNode = UUID_getNodeByUUID (p_uuid, p_iType)) == NULL)
        return NULL;
    return l_pListNode->sName;
}


SO_PUBLIC char *
UUID_Get_UUIDAsString (const char *p_sName, int p_iType)
{
    struct UUIDListNode *l_pListNode;
    char *l_sTemp;
    if ((l_pListNode = UUID_getNodeByName (p_sName, p_iType)) == NULL)
        return NULL;
    if ((l_sTemp = calloc (UUID_STRING_LENGTH, sizeof (char))) == NULL)
        return NULL;
    uuid_unparse (l_pListNode->uuid, l_sTemp);
    return l_sTemp;
}

SO_PUBLIC uint32_t
UUID_Get_Length (const char *p_sName, int p_iType)
{
    struct UUIDListNode *l_pListNode;
    if ((l_pListNode = UUID_getNodeByName (p_sName, p_iType)) == NULL)
        return 0;
    return l_pListNode->iLength;
}
