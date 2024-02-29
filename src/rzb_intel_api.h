#ifndef __RZB_INTEL_API_H__
#define __RZB_INTEL_API_H__

#include <rzb_api_types.h>

typedef struct _INTELAPI
{
    /* Standard */
    init_rzb_fp initRZB;
    fini_rzb_fp finiRZB;
    set_dbg_mode_fp setDebugMode;

    /* Intel */
    add_mail_data_fp addMailData;
    send_new_mail_fp sendNewMail;
    send_web_track_fp sendWebTrack;
    send_dns_track_fp sendDNSTrack;
    send_mail_attachment_fp sendMailAttachment;
} IntelAPI;

extern const IntelAPI rzb_intel;

#endif /* __RZB_INTEL_API_H__ */

