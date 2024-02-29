#include "config.h"
#include <razorback/debug.h>
#include <razorback/metadata.h>
#include <razorback/ntlv.h>
#include <razorback/uuids.h>
#include <razorback/log.h>

#include <string.h>

#if 0
SO_PUBLIC bool 
Metadata_Add (struct NTLVList *list, uuid_t name, uuid_t type, uint32_t size, uint8_t *data)
{
    return NTLVList_Add(list, name, type, size, data);
}
#endif

SO_PUBLIC bool 
Metadata_Add_String (struct NTLVList *list, uuid_t name, const char *string)
{
    uuid_t uuidType;
    if (!UUID_Get_UUID(NTLV_TYPE_STRING, UUID_TYPE_NTLV_TYPE, uuidType))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup type uuid", __func__);
        return false;
    }
    return Metadata_Add(list, name, uuidType, strlen(string), (uint8_t *)string);
}

SO_PUBLIC bool 
Metadata_Add_IPv4 (struct NTLVList *list, uuid_t name, uint8_t *addr)
{
    uuid_t uuidType;
    if (!UUID_Get_UUID(NTLV_TYPE_IPv4_ADDR, UUID_TYPE_NTLV_TYPE, uuidType))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup type uuid", __func__);
        return false;
    }
    return Metadata_Add(list, name, uuidType, 4, addr);
}

SO_PUBLIC bool 
Metadata_Add_IPv6 (struct NTLVList *list, uuid_t name, uint8_t *addr)
{
    uuid_t uuidType;
    if (!UUID_Get_UUID(NTLV_TYPE_IPv6_ADDR, UUID_TYPE_NTLV_TYPE, uuidType))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup type uuid", __func__);
        return false;
    }
    return Metadata_Add(list, name, uuidType, (128/8), addr);

    return true;
}

SO_PUBLIC bool 
Metadata_Add_Port (struct NTLVList *list, uuid_t name, uint16_t port)
{
    uuid_t uuidType;
    if (!UUID_Get_UUID(NTLV_TYPE_PORT, UUID_TYPE_NTLV_TYPE, uuidType))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup type uuid", __func__);
        return false;
    }
    return Metadata_Add(list, name, uuidType, 2, (uint8_t*)&port);
    return true;
}

SO_PUBLIC bool 
Metadata_Add_Filename (struct NTLVList *list, const char *name)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_FILENAME, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_String(list, uuidName, name);
}

SO_PUBLIC bool 
Metadata_Add_Hostname (struct NTLVList *list, const char *name)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_HOSTNAME, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_String(list, uuidName, name);
}

SO_PUBLIC bool 
Metadata_Add_URI (struct NTLVList *list, const char *name)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_URI, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_String(list, uuidName, name);
}

SO_PUBLIC bool 
Metadata_Add_HttpRequest (struct NTLVList *list, const char *name)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_HTTP_REQUEST, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_String(list, uuidName, name);
}

SO_PUBLIC bool 
Metadata_Add_HttpResponse (struct NTLVList *list, const char *name)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_HTTP_RESPONSE, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_String(list, uuidName, name);
}

SO_PUBLIC bool 
Metadata_Add_MalwareName (struct NTLVList *list, const char *vendor, const char *name)
{
    uuid_t uuidName;
    char *tmp;
    bool ret = true;

    if (!UUID_Get_UUID(NTLV_NAME_MALWARENAME, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    if (asprintf(&tmp, "%s:%s", vendor, name) == -1)
    {
        return false;
    }
    ret= Metadata_Add_String(list, uuidName,tmp);
    free(tmp);
    return ret;
}

SO_PUBLIC bool 
Metadata_Add_Report (struct NTLVList *list, const char *text)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_REPORT, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_String(list, uuidName, text);
}

SO_PUBLIC bool 
Metadata_Add_CVE (struct NTLVList *list, const char *text)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_CVE, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_String(list, uuidName, text);
}

SO_PUBLIC bool 
Metadata_Add_BID (struct NTLVList *list, const char *text)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_BID, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_String(list, uuidName, text);
}
SO_PUBLIC bool 
Metadata_Add_OSVDB (struct NTLVList *list, const char *text)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_OSVDB, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_String(list, uuidName, text);
}

SO_PUBLIC bool 
Metadata_Add_IPv4_Source (struct NTLVList *list, uint8_t *addr)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_SOURCE, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_IPv4(list, uuidName, addr);
}

SO_PUBLIC bool 
Metadata_Add_IPv4_Destination (struct NTLVList *list, uint8_t *addr)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_DEST, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_IPv4(list, uuidName, addr);
}

SO_PUBLIC bool 
Metadata_Add_IPv6_Source (struct NTLVList *list, uint8_t *addr)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_SOURCE, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_IPv6(list, uuidName, addr);
}

SO_PUBLIC bool 
Metadata_Add_IPv6_Destination (struct NTLVList *list, uint8_t *addr)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_DEST, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_IPv6(list, uuidName, addr);
}

SO_PUBLIC bool 
Metadata_Add_Port_Source (struct NTLVList *list, uint16_t addr)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_SOURCE, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_Port(list, uuidName, addr);
}

SO_PUBLIC bool 
Metadata_Add_Port_Destination (struct NTLVList *list, uint16_t addr)
{
    uuid_t uuidName;
    if (!UUID_Get_UUID(NTLV_NAME_DEST, UUID_TYPE_NTLV_NAME, uuidName))
    {
        rzb_log(LOG_ERR, "%s: Failed to lookup name uuid", __func__);
        return false;
    }
    return Metadata_Add_Port(list, uuidName, addr);
}

