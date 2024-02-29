#ifndef RAZORBACK_MESSAGES_CNC_CORE_H
#define RAZORBACK_MESSAGES_CNC_CORE_H
#include <razorback/messages.h>

struct Message * Message_CncCreate(uint32_t type, uint32_t version, size_t msgSize, const uuid_t source, const uuid_t dest);
struct Message * Message_CncBcastCreate(uint32_t type, uint32_t version, size_t msgSize, const uuid_t source);

bool Message_CnC_Deserialize_Empty(struct Message *message, int mode);
bool Message_CnC_Serialize_Empty(struct Message *message, int mode);
bool Message_CnC_Setup(struct Message *message);

void Message_CnC_Bye_Setup(struct Message *message);
void Message_CnC_CacheClear_Setup(struct Message *message);
void Message_CnC_ConfigAck_Setup(struct Message *message);
void Message_CnC_ConfigUpdate_Setup(struct Message *message);
void Message_CnC_Error_Setup(struct Message *message);
void Message_CnC_Go_Setup(struct Message *message);
void Message_CnC_Hello_Setup(struct Message *message);
void Message_CnC_Pause_Setup(struct Message *message);
void Message_CnC_Paused_Setup(struct Message *message);
void Message_CnC_RegReq_Setup(struct Message *message);
void Message_CnC_RegResp_Setup(struct Message *message);
void Message_CnC_Running_Setup(struct Message *message);
void Message_CnC_Term_Setup(struct Message *message);

#endif
