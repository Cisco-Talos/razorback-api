#ifndef __RZB_COLLECTOR_API_H__
#define __RZB_COLLECTOR_API_H__

#include <rzb_api_types.h>

typedef struct _COLLECTIONAPI
{
    /* Standard */
    init_rzb_fp initRZB;
    fini_rzb_fp finiRZB;
    set_dbg_mode_fp setDebugMode;

    /* Collection */
    register_nugget_fp registerNugget;
    check_resource_fp checkResource;
    send_data_fp sendData;
    send_meta_data_fp sendMetaData;
    file_type_lookup_fp file_type_lookup;

    /* Intel */
    add_mail_data_fp addMailData;
    send_new_mail_fp sendNewMail;
    send_web_track_fp sendWebTrack;
    send_dns_track_fp sendDNSTrack;
    send_mail_attachment_fp sendMailAttachment;
} CollectionAPI;

extern const CollectionAPI rzb_collection;

#endif  /* __RZB_COLLECTOR_API_H__ */

