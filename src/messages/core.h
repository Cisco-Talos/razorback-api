#ifndef RAZORBACK_MESSAGES_CORE_H
#define RAZORBACK_MESSAGES_CORE_H
#include <razorback/messages.h>

struct Message * Message_Create(uint32_t type, uint32_t version, size_t msgSize);
void Message_Destroy(struct Message *message);
bool Message_Setup(struct Message *message);

void MessageBlockSubmission_Setup(struct Message *msg);
void MessageCacheReq_Setup(struct Message *msg);
void MessageCacheResp_Setup(struct Message *msg);
void MessageInspectionSubmission_Setup(struct Message *msg);
void MessageJudgmentSubmission_Setup(struct Message *msg);
void MessageLogSubmission_Setup(struct Message *msg);

#endif
