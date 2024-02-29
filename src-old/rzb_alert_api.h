/** @file rzb_alert_api.h
 * Razorback Alerting API.
 */

#ifndef RZB_ALERT_API_H
#define RZB_ALERT_API_H

#include "rzb_api_types.h"

/* Protocol Additions to Support Component Management */
typedef struct _BLOB_REQUEST
{
    unsigned      alertID;
    unsigned      blob_type;
} BLOB_REQUEST;

typedef struct _BLOB_HEADER
{
    unsigned      alertID;
    unsigned      blobType;
    unsigned      blobSize;
    unsigned      flags; // Review for use
} BLOB_HEADER;

/* Public available API functions */
HRESULT deliverJudgement(JUDGEMENT *verdict);
HRESULT getAlertData(unsigned alertID, unsigned blob_type, unsigned char **data, unsigned *size);

/** Submit an alert to the dispatcher.
 * @param *alert An ::_ALERT containing the alert data.
 * @return A value from ::HRESULT
 */
HRESULT sendAlert(ALERT *alert);

#endif /* RZB_ALERT_API_H */
