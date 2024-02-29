#ifndef RZB_INTEL_GLOBAL_H
#define RZB_INTEL_GLOBAL_H

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "rzb_api_types.h"

/* STRINGS */
#define NULL_ENTRY "NULL"

/* LIMITS */
#define MAX_MAIL_FROM_SIZE    1024
#define MAX_RCPT_TO_SIZE      1024
#define MAX_SUBJECT_SIZE      1024
#define MAX_MSG_FROM_SIZE     1024
#define MAX_MSG_TO_SIZE       1024

/* INTEL DATA TYPES */
typedef enum
{
    NEWDNS,
    NEWWEB,
    NEWMAIL,
    ADD_MAIL_DATA,
    ADD_MAIL_ATTACHMENT
} INTEL_MESSAGE;

/* INTEL TRACKING */
typedef struct _INTEL_TYPE
{
    unsigned msg_type;
} INTEL_TYPE;

/* DNS TRACKING */
typedef struct _DNS_ENTRY
{
    RZB_IP src_ip;
    RZB_IP dst_ip;
    unsigned short tx_id;
    unsigned short flags;
    char *queries;
    char *answers;
    char *auth_ns;
    char *add_rec;
} DNS_ENTRY;

typedef struct _DNS_TRACK
{
    RZB_IP src_ip;
    RZB_IP dst_ip;
    unsigned short tx_id;
    unsigned short flags;
    unsigned queries_size;
    unsigned answers_size;
    unsigned auth_ns_size;
    unsigned add_rec_size;
} DNS_TRACK;

/* WEB TRACKING */
typedef struct _WEB_FINAL
{
    RZB_IP src_ip;
    RZB_IP dst_ip;
    char *url;
    char *host;
    char *user_agent;
    char *cookie;
    char *header;
} WEB_FINAL;

typedef struct _WEB_TRACK
{
    RZB_IP src_ip;
    RZB_IP dst_ip;
    unsigned url_size;
    unsigned host_size;
    unsigned user_agent_size;
    unsigned cookie_size;
    unsigned header_size;
} WEB_TRACK;

/* MAIL TRACKING */

typedef struct _ATTACHMENT_INFO
{
    unsigned      mail_id;
    unsigned char     hash[RZB_HASH_SIZE];
    unsigned      size;
    uuid_t            data_type;
    unsigned      name_size;
} ATTACHMENT_INFO;

typedef struct _MAIL_UPDATE
{
    unsigned mail_id;
    unsigned info_type;
    unsigned info_size;
} MAIL_UPDATE;

typedef struct _NEW_MAIL
{
    RZB_IP src_ip;
    RZB_IP dst_ip;
    unsigned   mail_from_size;
    unsigned   rcpt_to_size;
    unsigned   subject_size;
    unsigned   msg_from_size;
    unsigned   msg_to_size;
} NEW_MAIL;

typedef struct _NEW_MAIL_RESPONSE
{
    unsigned mail_id;
} NEW_MAIL_RESPONSE;

/* OTHER DATA STRUCTURES */
typedef struct _NEW_MAIL_BLOBS
{
    char *mail_from;
    char *rcpt_to;
    char *subject;
    char *msg_from;
    char *msg_to;
} NEW_MAIL_BLOBS;

HRESULT addMailData(unsigned datatype, const char *data, unsigned mail_id);
HRESULT sendNewMail(const BASE_MAIL_DATA *mail, unsigned *mail_id);
HRESULT sendWebTrack(WEB_ENTRY *web_cap);
HRESULT sendDNSTrack(const DNS_TRACK_DATA *dns_cap);
HRESULT sendMailAttachment(unsigned size, const unsigned char *data, const unsigned char *datatype,
                           const char *name, unsigned mail_id);

#endif /* RZB_INTEL_GLOBAL_H */

