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

/** @file uuids.h
 * UUID List API.
 */
#ifndef RAZORBACK_UUIDS_H
#define RAZORBACK_UUIDS_H

#include <uuid/uuid.h>

#include <razorback/visibility.h>
#include <razorback/types.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * UUID list entry.
 */
struct UUIDListNode
{
    uuid_t uuid;
    char *sName;
    char *sDescription;
    struct UUIDListNode *pNext;
};
/** UUID Types
 * @{
 */
#define UUID_TYPE_DATA_TYPE 1   ///< Data Type
#define UUID_TYPE_INTEL_TYPE 2  ///< Intel Type
#define UUID_TYPE_NTLV_TYPE 3   ///< NTLV Type
#define UUID_TYPE_NUGGET 4      ///< Nugget
#define UUID_TYPE_NUGGET_TYPE 5 ///< Nugget Type
#define UUID_TYPE_NTLV_NAME 6   ///< NTLV Name
/// @}
//


/** Well Known UUID's
 * @{
 */
#define NUGGET_TYPE_CORRELATION "CORRELATION"
#define NUGGET_TYPE_INTEL "INTEL"
#define NUGGET_TYPE_DEFENSE "DEFENSE"
#define NUGGET_TYPE_OUTPUT "OUTPUT"
#define NUGGET_TYPE_COLLECTION "COLLECTION"
#define NUGGET_TYPE_INSPECTION "INSPECTION"
#define NUGGET_TYPE_MASTER "MASTER"
#define NUGGET_TYPE_DISPATCHER "DISPATCHER"



#define DATA_TYPE_7ZIP_FILE "7ZIP_FILE"
#define DATA_TYPE_ANY_DATA "ANY_DATA"
#define DATA_TYPE_AR_FILE "application/x-archive"

#define DATA_TYPE_BZ2_FILE "application/x-bzip2"
#define DATA_TYPE_CAB_FILE "application/vnd.ms-cab-compressed"
#define DATA_TYPE_COMPRESSION_FILE "application/x-compress"
#define DATA_TYPE_CPIO_FILE "application/x-cpio"
#define DATA_TYPE_ELF_FILE "ELF_FILE"
#define DATA_TYPE_FLASH_FILE "FLASH_FILE"
#define DATA_TYPE_GRZIP_FILE "GRZIP_FILE"
#define DATA_TYPE_GZIP_FILE "application/gzip"
#define DATA_TYPE_ISO9660_FILE "application/x-iso9660"
#define DATA_TYPE_JAR_FILE "application/java-archive"
#define DATA_TYPE_JAVASCRIPT "text/javascript"
#define DATA_TYPE_LHA_FILE "LHA_FILE"
#define DATA_TYPE_LRZIP_FILE "LRZIP_FILE"
#define DATA_TYPE_LZ4_FILE "LZ4_FILE"
#define DATA_TYPE_LZMA_FILE "LZMA_FILE"
#define DATA_TYPE_LZOP_FILE "LZOP_FILE"
#define DATA_TYPE_MTREE_FILE "MTREE_FILE"
#define DATA_TYPE_OLE_FILE "OLE_FILE"
#define DATA_TYPE_PAR2_FILE "PAR2_FILE"
#define DATA_TYPE_PAR_FILE "PAR_FILE"
#define DATA_TYPE_PDF_FILE "application/pdf"
#define DATA_TYPE_PE_FILE "application/x-dosexe"
#define DATA_TYPE_RAR_FILE "application/x-rar"
#define DATA_TYPE_RPM_FILE "application/x-rpm"
#define DATA_TYPE_SHELL_CODE "SHELL_CODE"
#define DATA_TYPE_SMTP_CAPTURE "SMTP_CAPTURE"
#define DATA_TYPE_TAR_FILE "application/x-tar"
#define DATA_TYPE_UU_FILE "text/x-uuencode"
#define DATA_TYPE_XAR_FILE "XAR_FILE"
#define DATA_TYPE_XZ_FILE "application/x-xz"
#define DATA_TYPE_ZIP_FILE  "application/zip"
#define DATA_TYPE_ZLIB_FILE "application/zlib"

#define NTLV_NAME_SOURCE "SOURCE"
#define NTLV_NAME_DEST "DEST"
#define NTLV_NAME_FILENAME "FILENAME"
#define NTLV_NAME_HOSTNAME "HOSTNAME"
#define NTLV_NAME_PATH "PATH"
#define NTLV_NAME_MALWARENAME "MALWARENAME"
#define NTLV_NAME_REPORT "REPORT"
#define NTLV_NAME_CVE "CVE"
#define NTLV_NAME_BID "BID"
#define NTLV_NAME_OSVDB "OSVDB"
#define NTLV_NAME_URI "URI"
#define NTLV_NAME_HTTP_REQUEST "HTTP_REQUEST"
#define NTLV_NAME_HTTP_RESPONSE "HTTP_RESPONSE"

#define NTLV_TYPE_IPv6_ADDR "IPv6_ADDR"
#define NTLV_TYPE_IPv4_ADDR "IPv4_ADDR"
#define NTLV_TYPE_JSON "JSON"
#define NTLV_TYPE_PORT "PORT"
#define NTLV_TYPE_STRING "STRING"
#define NTLV_TYPE_IPPROTO "IPPROTO"

/// @}

/**
 * Get the UUID for the listed name and type.
 * @param p_sName The UUID name.
 * @param p_iType The UUID type.
 * @param r_uuid The place to put the uuid.
 * @return true on success false on error.
 */
SO_PUBLIC extern bool UUID_Get_UUID(const char *p_sName, int p_iType, uuid_t r_uuid);

/**
 * Get the description for the listed name and type.
 * The string should be free'd when its finished with.
 * @param p_sName The UUID name.
 * @param p_iType The UUID type.
 * @return NULL on error.
 */
SO_PUBLIC extern char * UUID_Get_Description(const char *p_sName, int p_iType);

/**
 * Get the name for the listed uuid and type.
 * The string should be free'd when its finished with.
 * @param p_uuid The UUID.
 * @param p_iType The UUID type.
 * @return NULL on error.
 */
SO_PUBLIC extern char * UUID_Get_NameByUUID(uuid_t p_uuid, int p_iType);

/**
 * Get the description for the listed uuid and type.
 * The string should be free'd when its finished with.
 * @param p_uuid The UUID.
 * @param p_iType The UUID type.
 * @return NULL on error.
 */
SO_PUBLIC extern char * UUID_Get_DescriptionByUUID(uuid_t p_uuid, int p_iType);

/**
 * Get the UUID in string form for the listed name and type.
 * The string should be free'd when its finished with.
 * @param p_sName The UUID name.
 * @param p_iType The UUID type.
 * @return NULL on error.
 */
SO_PUBLIC extern char * UUID_Get_UUIDAsString(const char *p_sName, int p_iType);

/**
 * Create a UUID list.
 * @return Requested object on success, or NULL on failure.
 */
SO_PUBLIC extern List_t * UUID_Create_List(void);

/**
 * Add an entry to a UUID list.
 * @param list List to operate on.
 * @param uuid UUID value.
 * @param name Name string.
 * @param desc Desc string.
 * @return true on success, false on failure.
 */
SO_PUBLIC extern bool UUID_Add_List_Entry(
    List_t *list,
    uuid_t uuid,
    const char *name,
    const char *desc
);

/**
 * Get the UUID list for a type.
 * @param type Type value.
 * @return Requested object on success, or NULL on failure.
 */
SO_PUBLIC extern List_t * UUID_Get_List(int type);

/**
 * Get the serialized size of a UUID list.
 * @param list List to operate on.
 * @return Requested size value.
 */
SO_PUBLIC extern size_t UUIDList_BinarySize(List_t *list);

#ifdef __cplusplus
}
#endif
#endif
