#ifndef __RZB_DETECTION_API_H__
#define __RZB_DETECTION_API_H__

#include <rzb_api_types.h>

typedef struct _DETECTIONAPI
{
    /* Standard */
    init_rzb_fp initRZB;
    fini_rzb_fp finiRZB;
    set_dbg_mode_fp setDebugMode;

    /* Detection */
    nugget_server_fp nuggetServer;
    reg_handler_fp registerHandler;
    send_alert_fp sendAlert;
    alerts_done_fp alertsDone;
    hash_data_fp hashData;
    hash_data_to_string_fp hashDataToString;
    deliver_judgement_fp deliverJudgement;
    get_alert_data_fp getAlertData;

    /* Collection */
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
} DetectionAPI;

extern const DetectionAPI rzb_detection;

#endif  /* __RZB_DETECTION_API_H__ */

