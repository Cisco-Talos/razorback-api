/** @file uuids.h
 * UUID List API.
 */
#ifndef RAZORBACK_UUIDS_H
#define RAZORBACK_UUIDS_H

#include <uuid/uuid.h>
#include <razorback/types.h>

#include <pthread.h>
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

struct UUIDListNode
{
    uuid_t uuid;
    char *sName;
    char *sDescription;
    struct UUIDListNode *pNext;
};

struct UUIDList
{
    struct UUIDListNode *pHead;
    struct UUIDListNode *pTail;
    pthread_mutex_t mutex;
    uint32_t count;
};

/** Well Known UUID's
 * @{
 */
#define NUGGET_TYPE_CORRELATION "CORRELATION"
#define NUGGET_TYPE_INTEL "INTEL"
#define NUGGET_TYPE_DEFENSE "DEFENSE"
#define NUGGET_TYPE_OUTPUT "OUTPUT"
#define NUGGET_TYPE_COLLECTION "COLLECITON"
#define NUGGET_TYPE_INSPECTION "INSPECTION"
#define NUGGET_TYPE_MASTER "MASTER"
#define NUGGET_TYPE_DISPATCHER "DISPATCHER"

#define DATA_TYPE_ANY_DATA "ANY_DATA"
#define DATA_TYPE_FLASH_FILE "FLASH_FILE"
#define DATA_TYPE_JAVASCRIPT "JAVASCRIPT"
#define DATA_TYPE_OLE_FILE "OLE_FILE"
#define DATA_TYPE_PAR2_FILE "PAR2_FILE"
#define DATA_TYPE_PAR_FILE "PAR_FILE"
#define DATA_TYPE_PDF_FILE "PDF_FILE"
#define DATA_TYPE_PE_FILE "PE_FILE"
#define DATA_TYPE_RAR_FILE "RAR_FILE"
#define DATA_TYPE_SHELL_CODE "SHELL_CODE"
#define DATA_TYPE_SMTP_CAPTURE "SMTP_CAPTURE"
#define DATA_TYPE_TAR_FILE "TAR_FILE"
#define DATA_TYPE_ZIP_FILE "ZIP_FILE"
#define DATA_TYPE_BZ2_FILE "BZ2_FILE"
#define DATA_TYPE_GZIP_FILE "GZIP_FILE"
#define DATA_TYPE_COMPRESSION_FILE "COMPRESSION_FILE"
#define DATA_TYPE_LZMA_FILE "LZMA_FILE"
#define DATA_TYPE_XZ_FILE "XZ_FILE"

#define DATA_TYPE_AR_FILE "AR_FILE"
#define DATA_TYPE_CPIO_FILE "CPIO_FILE"
#define DATA_TYPE_ISO9660_FILE "ISO9660_FILE"



#define NUGGET_ARCHIVE_INFLATE  "ARCHIVE_INFLATE"
#define NUGGET_CLAMAV "CLAMAV"
#define NUGGET_FLASH_INSPECTOR "FLASH_INSPECTOR"
#define NUGGET_FILE_INJECT "FILE_INJECT"
#define NUGGET_FS_MONITOR "FS_MONITOR"
#define NUGGET_FS_WALKER "FS_WALKER"
#define NUGGET_LIBEMU "LIBEMU"
#define NUGGET_OFFICECAT "OFFICECAT"
#define NUGGET_PDF_DISSECTOR "PDF_DISSECTOR"
#define NUGGET_POSTFIX "POSTFIX"
#define NUGGET_SENDMAIL_MIlTER "SENDMAIL_MIlTER"
#define NUGGET_SMTP_PARSER "SMTP_PARSER"
#define NUGGET_SNORT_COLLECTOR "SNORT_COLLECTOR"
#define NUGGET_SQUID_COLLECTOR "SQUID_COLLECTOR"
#define NUGGET_STEG_DETECT "STEG_DETECT"
#define NUGGET_VIRUS_TOTAL "VIRUS_TOTAL"
#define NUGGET_YARA "YARA"
#define NUGGET_FILE_LOG "FILE_LOG"
#define NUGGET_PDF1_LOG "PDF_LOG_1"
#define NUGGET_PDF2_LOG "PDF_LOG_2"
#define NUGGET_PDF3_LOG "PDF_LOG_3"
#define NUGGET_SWF1_LOG "SWF_LOG_1"
#define NUGGET_SWF2_LOG "SWF_LOG_2"

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

#define NTLV_TYPE_IPv6_ADDR "IPv6_ADDR"
#define NTLV_TYPE_IPv4_ADDR "IPv4_ADDR"
#define NTLV_TYPE_PORT "PORT"
#define NTLV_TYPE_STRING "STRING"
#define NTLV_TYPE_IPPROTO "IPPROTO"

/// @}

/** Get the UUID for the listed name and type
 * @param p_sName The UUID name
 * @param p_uType The UUID type
 * @param r_uuid The place to put the uuid
 * @return true on success false on error
 */
extern bool UUID_Get_UUID (const char *p_sName, int p_iType, uuid_t r_uuid);

/** Get the description for the listed name and type
 * The string should be free'd when its finished with.
 * @param p_sName The UUID name
 * @param p_uType The UUID type
 * @return NULL on error
 */
extern char *UUID_Get_Description (const char *p_sName, int p_iType);

/** Get the name for the listed uuid and type
 * The string should be free'd when its finished with.
 * @param p_uuid The UUID
 * @param p_iType The UUID type
 * @return NULL on error
 */

extern char *UUID_Get_NameByUUID (uuid_t p_uuid, int p_iType);
/** Get the description for the listed uuid and type
 * The string should be free'd when its finished with.
 * @param p_uuid The UUID
 * @param p_iType The UUID type
 * @return NULL on error
 */
extern char *UUID_Get_DescriptionByUUID (uuid_t p_uuid, int p_iType);

/** Get the UUID in string form for the listed name and type
 * The string should be free'd when its finished with.
 * @param p_sName The UUID name
 * @param p_uType The UUID type
 * @return NULL on error
 */
extern char *UUID_Get_UUIDAsString (const char *p_sName, int p_iType);

extern bool UUID_Get_Next_UUID (uuid_t p_uuid, int p_iType, uuid_t r_uuid);
extern bool UUID_Get_First_UUID (int p_iType, uuid_t r_uuid);

extern struct UUIDList * UUID_Create_List (void);
extern bool UUID_Add_List_Entry(struct UUIDList *list, uuid_t uuid, const char *name, const char *desc);

extern struct UUIDList * UUID_Get_List(int type);
extern void UUID_Clear_List(struct UUIDList *list);
#endif
