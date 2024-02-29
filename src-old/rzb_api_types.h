/** @file rzb_api_types.h
 * Razorback API Types.
 */
#ifndef RZB_API_TYPES_H
#define RZB_API_TYPES_H

#include <netinet/in.h>
#if defined(__FreeBSD__)
#include <sys/socket.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#include "rzb_utils_types.h"

/* Well known Nugget Information */

/* Nugget type: Correlation */
UUID_DEFINE(CORRELATION, 0x2f, 0xd7, 0x5f, 0xa5, 0x77, 0x8b, 0x44, 0x3e, 0xb9, 0x10, 0x1e, 0x19, 0x04, 0x4e, 0x81, 0xe1);
/* Nugget type: Detection */
UUID_DEFINE(DETECTION, 0xd9, 0x5a, 0xee, 0x72, 0x91, 0x86, 0x42, 0x36, 0xbf, 0x23, 0x8f, 0xf7, 0x7d, 0xac, 0x63, 0x0b);
/* Nugget type: Output */
UUID_DEFINE(OUTPUT, 0xa3, 0xd0, 0xd1, 0xf9, 0xc0, 0x49, 0x47, 0x4e, 0xbf, 0x01, 0x21, 0x28, 0xea, 0x00, 0xa7, 0x51);
/* Nugget type: Data Collector */
UUID_DEFINE(COLLECTOR, 0xc3, 0x8b, 0x11, 0x3a, 0x27, 0xfd, 0x41, 0x7c, 0xb9, 0xfa, 0xf3, 0xaa, 0x0a, 0xf5, 0xcb, 0x53);
/* Nugget type: Intel */
UUID_DEFINE(INTEL, 0x35, 0x61, 0x12, 0xd8, 0xf4, 0xf1, 0x41, 0xdc, 0xb3, 0xf7, 0xca, 0xce, 0x56, 0x74, 0xc2, 0xec);

/* Well know Data Types */
UUID_DEFINE(NO_DATA_TYPE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
UUID_DEFINE(PDF_FILE, 0x00, 0x5d, 0x54, 0x64, 0x7a, 0x44, 0x49, 0x07, 0xaf, 0x57, 0x4d, 0xb0, 0x8a, 0x61, 0xe1, 0x3c);
UUID_DEFINE(SWF_FILE, 0x7a, 0xb4, 0x5f, 0xff, 0x7c, 0x73, 0x41, 0x2c, 0x8b, 0x86, 0xc0, 0x76, 0x19, 0xc8, 0xfc, 0x7d);
UUID_DEFINE(ZIP_FILE, 0x52, 0xf7, 0x38, 0xc2, 0xdb, 0x1c, 0x42, 0xca, 0xb5, 0xef, 0x50, 0xa4, 0xba, 0x3f, 0x75, 0x27);
UUID_DEFINE(PE_FILE, 0xba, 0x9b, 0xeb, 0x5f, 0x06, 0x53, 0x4b, 0x04, 0x95, 0x52, 0x3b, 0xfb, 0x63, 0x4c, 0xa7, 0xfc);
UUID_DEFINE(OLE_DOC, 0x16, 0xd7, 0x29, 0x48, 0x3d, 0x2b, 0x52, 0xed, 0x9a, 0xe4, 0x43, 0xef, 0x19, 0xce, 0x3e, 0x69);
UUID_DEFINE(MAIL_CAPTURE, 0xd1, 0x47, 0xf2, 0x15, 0x12, 0x8e, 0x47, 0x46, 0xa1, 0xe2, 0xb6, 0xc9, 0x78, 0xbb, 0x18, 0x69);
UUID_DEFINE(WEB_CAPTURE, 0x5d, 0x1a, 0x26, 0xc7, 0xff, 0xa2, 0x40, 0x67, 0x9a, 0xf8, 0x4e, 0xd9, 0xee, 0x39, 0x38, 0x05);
UUID_DEFINE(DNS_CAPTURE, 0x1d, 0x69, 0x35, 0x1c, 0xbc, 0x4c, 0x56, 0xe1, 0x81, 0x40, 0x0d, 0x83, 0x84, 0xe6, 0x14, 0x77);
UUID_DEFINE(ALERT_OUTPUT, 0x9b, 0xfc, 0x66, 0x6d, 0xc3, 0xd8, 0x55, 0xcc, 0xa2, 0xa5, 0x4d, 0x66, 0xd5, 0xa5, 0x0c, 0x59);
UUID_DEFINE(CAP_DESCRIP, 0x3d, 0x39, 0x89, 0x3e, 0xae, 0x97, 0x53, 0x37, 0xac, 0x35, 0x0a, 0xbe, 0x30, 0x75, 0xfb, 0x88);
UUID_DEFINE(SHELLCODE, 0x4e, 0x72, 0xc8, 0xec, 0xff, 0x88, 0x43, 0x71, 0xa0, 0xf0, 0xdf, 0xe2, 0xb4, 0xc7, 0x33, 0xdc);


/* Cache Codes */
#define RZB_CACHE_HIT_KNOWN_BAD   1
#define RZB_CACHE_HIT_KNOWN_GOOD  2

#define SIZE_OF_UUID_STRING  37

#define MAX_MSG_SIZE          256
#define MAX_SHORT_SIZE        2048
#define MAX_LONG_SIZE         16777215

/* THREAT FLAGS */
#define KNOWN_GOOD   0x00000001
#define KNOWN_BAD    0x00000002
#define THREAT       0x00000004
#define HAZZARD      0x00000008
#define WHITE_LIST   0x00000010
#define TAINT        0x00000020
#define WATCH_LIST   0x00000040

/* Definitions to support ALERT_PACKET */
#define SHORT_DATA      0x00000001
#define LONG_DATA       0x00000002
#define FULL_BLOCK      0x00000004
#define NORM_BLOCK      0x00000008
#define SUB_COMPONENT   0x00000010
#define META_DATA       0x00000020
#define ALRT_MESSAGE    0x00000040     // NOT USED BY NORMAL ALERTING PROCESS

typedef enum
{
    HEADER_DATE,
    MSG_ID,
    MIME_VER,
    LANGUAGE,
    MAILER,
    THREAD_IDX,
    BODY,
    XBLOCK,
    MAX_ADD_MAIL_DATA_TYPE
} ADD_MAIL_DATA_TYPES;

/*
 *
 * NETWORK INFO
 *
 */

typedef struct _METAPACKET
{
    unsigned type;
    unsigned filesize;
    unsigned char hash[RZB_HASH_SIZE];
    unsigned size;
} METAPACKET;

/** An alert to be submitted to the API.
 * Pointer values should be set to NULL if they are not being
 * submitted with the alert. Size values that are not used 
 * should be set to zero.
 * 
 */
typedef struct _ALERT
{
    /// @name Required members
    /// @{
    unsigned event_id;              ///< The ID of the event that triggered this alert.
    unsigned short priority;        ///< The pro
    char src_ip[INET6_ADDRSTRLEN];  ///< The source IP of the event
    char dst_ip[INET6_ADDRSTRLEN];  ///< The destination IP of the event
    unsigned short ip_proto;        ///< The IP protocol of the event
    const char *msg;                ///< The message that discribes the event
    unsigned msg_size;              ///< The length of #msg
    const char *short_data;         ///< The short data from the event.
    unsigned sd_size;               ///< The length of #short_data
    uuid_t dataType;                ///< The type of data that generated this event.
    /** @}
     * @name Optional members
     * @{
     */
    unsigned short src_port;        ///< The port on #src_ip that generated the alert.
    unsigned short dst_port;        ///< The port on #dst_ip that generated the alert.
    unsigned char *main_hash;       ///< 
    const char *long_data;          ///< The long data from the event.
    unsigned ld_size;               ///< The length of #long_data
    const void *data_block;         ///< The data block from the event.
    unsigned db_size;               ///< The size of #data_block
    const void *norm_block;         ///< The normalised data from the event.
    unsigned norm_size;             ///< The soe of #norm_block
    /// @}
} ALERT;

typedef struct _JUDGEMENT
{
    unsigned char hash[RZB_HASH_SIZE];
    unsigned size;
    unsigned flags;
} JUDGEMENT;

typedef struct _BASE_MAIL_DATA
{
    char *src_ip;
    char *dst_ip;
    char *mail_from;  // NOT REQUIRED
    char *rcpt_to;    // NOT REQUIRED
    char *subject;    // NOT REQUIRED
    char *msg_from;   // NOT REQUIRED
    char *msg_to;     // NOT REQUIRED
} BASE_MAIL_DATA;

typedef struct _WEB_ENTRY
{
    RZB_IP src_ip;
    RZB_IP dst_ip;
    char url[1024];
    char host[1024];
    char user_agent[1024];
    char cookie[4096];
    char header[4096];
} WEB_ENTRY;

typedef struct _DNS_TRACK_DATA
{
    char *src_ip;
    char *dst_ip;
    unsigned short tx_id;
    unsigned short flags;
    char *queries;  // NOT REQUIRED
    char *answers;  // NOT REQUIRED
    char *auth_ns;  // NOT REQUIRED
    char *add_rec;  // NOT REQUIRED
} DNS_TRACK_DATA;

typedef struct _ALERT_HEADER
{
    unsigned alert_id;          // Filled in after db insert
    unsigned timestamp;
    unsigned event_id;          // Standard event data
    unsigned nugget_id;
    unsigned short priority;
    RZB_IP src_ip;
    RZB_IP dst_ip;
    unsigned short ip_proto;
    unsigned short src_port;
    unsigned short dst_port;
    unsigned flags1;            // Declare additional information available
    unsigned flags2;            // reserved
    unsigned char main_hash[RZB_HASH_SIZE]; // Subcompoments will have different_md5
    uuid_t dataType;
    unsigned char db_hash[RZB_HASH_SIZE];
    unsigned db_size;
    unsigned char norm_hash[RZB_HASH_SIZE];
    unsigned norm_size;
} ALERT_HEADER;

typedef struct _ROUTE_ENTRY_BLOCK
{
    unsigned nugid;
    char nugaddr[INET_ADDRSTRLEN];
    unsigned short nugport;
    char *name;
    struct _ROUTE_ENTRY_BLOCK *next;
} ROUTE_ENTRY_BLOCK;

typedef struct _APP_ENTRY_BLOCK
{
    char apptype[SIZE_OF_UUID_STRING];
    char *name;
    ROUTE_ENTRY_BLOCK *nug_list;
    struct _APP_ENTRY_BLOCK *next;
} APP_ENTRY_BLOCK;

typedef struct _DATA_ENTRY_BLOCK
{
    char datatype[SIZE_OF_UUID_STRING];
    char *name;
    APP_ENTRY_BLOCK *app_list;
    struct _DATA_ENTRY_BLOCK *next;
} DATA_ENTRY_BLOCK;

typedef struct _LOG_BLOCK
{
    unsigned id;        // Log_ID primary key from framework_logs table
    unsigned timestamp; // Seconds since epoch
    unsigned level;     // Syslog level declarations
    unsigned char *src;     // Name of the function submitting the log entry
    unsigned char *msg;     // Text of the log message
    struct _LOG_BLOCK *next; // Pointer to the next log block, NULL is end of list
} LOG_BLOCK;

/*
 *
 * INTERFACE OBJECTS
 *
 */

struct _DETECTIONAPI;
typedef HRESULT (*initNugFunc)(const struct _DETECTIONAPI *);

typedef HRESULT (*init_rzb_fp)(const char *conf_filename);
typedef void (*fini_rzb_fp)(unsigned to_secs);
typedef void (*set_dbg_mode_fp)(int mode);

typedef HRESULT (*register_nugget_fp)(const uuid_t nugtype, const uuid_t nugapp, const uuid_t datatype,
                                      unsigned short nugport, unsigned instances, unsigned nugid,
                                      const char *name, unsigned *nuggetid);
typedef HRESULT (*check_resource_fp)(const uuid_t id, const char *data, const uuid_t type);
typedef HRESULT (*send_data_fp)(BLOCK_META_DATA *metaData);
typedef HRESULT (*send_meta_data_fp)(METAPACKET *mpacket, unsigned char *data);
typedef const unsigned char *(*file_type_lookup_fp)(const void *data, size_t len);

typedef HRESULT (*nugget_server_fp)(const char *configFile);
typedef void (*handler_fp)(BLOCK_META_DATA *metData);
typedef HRESULT (*reg_handler_fp)(handler_fp fp,
                                  const uuid_t *acceptedTypes,
                                  size_t numTypes,
                                  const uuid_t nuggetType);
typedef HRESULT (*send_alert_fp)(ALERT *alert);
typedef HRESULT (*alerts_done_fp)(unsigned eventid);
typedef void (*hash_data_fp)(const void *content, ssize_t len, unsigned char *hash);
typedef char *(*hash_data_to_string_fp)(const void *content, ssize_t len);
typedef HRESULT (*deliver_judgement_fp)(JUDGEMENT *verdict);
typedef HRESULT (*get_alert_data_fp)(unsigned alertID, unsigned blob_type,
                                     unsigned char **data, unsigned *size);

typedef HRESULT (*add_mail_data_fp)(unsigned datatype, const char *data, unsigned mail_id);
typedef HRESULT (*send_new_mail_fp)(const BASE_MAIL_DATA *mail, unsigned *mail_id);
typedef HRESULT (*send_web_track_fp)(WEB_ENTRY *web_cap);
typedef HRESULT (*send_dns_track_fp)(const DNS_TRACK_DATA *dns_cap);
typedef HRESULT (*send_mail_attachment_fp)(unsigned size, const unsigned char *data, const unsigned char *datatype,
                                           const char *name, unsigned mail_id);

typedef HRESULT (*get_route_table_fp)(DATA_ENTRY_BLOCK **route_table);
typedef HRESULT (*get_logs_by_num_fp)(LOG_BLOCK **logs, unsigned num);

#endif /* RZB_API_TYPES_H */

