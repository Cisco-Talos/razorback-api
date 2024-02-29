/** @file api.h
 * Razorback API.
 */
#ifndef RAZORBACK_API_H
#define RAZORBACK_API_H

#include <razorback/types.h>
#include <razorback/messages.h>
#include <semaphore.h>

#define DECL_INSPECTION_FUNC(a) uint8_t a (struct Block *block, struct EventId *eventId, struct NTLVList *eventMetadata)
/** Inspection Nugget Hooks
 */
struct RazorbackInspectionHooks
{
    uint8_t (*processBlock) (struct Block *, struct EventId *, struct NTLVList *);    ///< FP to inspection handler
    bool (*processDeferredList) (struct DeferredList *);            ///< FP to pending items

};

struct RazorbackCommandAndControlHooks
{
    bool (*processRegReqMessage) (struct MessageRegistrationRequest *);
    bool (*processRegRespMessage) (struct MessageRegistrationResponse *);
    bool (*processRegErrMessage) (struct MessageError *);
    bool (*processConfUpdateMessage) (struct MessageConfigurationUpdate *);
    bool (*processConfAckMessage) (struct MessageConfigurationAck *);
    bool (*processConfErrMessage) (struct MessageError *);
    bool (*processStatsReqMessage) (struct MessageStatsRequest *);
    bool (*processStatsRespMessage) (struct MessageStatsResponse *);
    bool (*processStatsErrMessage) (struct MessageError *);
    bool (*processPauseMessage) (struct MessagePause *);
    bool (*processPausedMessage) (struct MessagePaused *);
    bool (*processGoMessage) (struct MessageGo *);
    bool (*processRunningMessage) (struct MessageRunning *);
    bool (*processTermMessage) (struct MessageTerminate *);
    bool (*processByeMessage) (struct MessageBye *);
};

#define CONTEXT_FLAG_STAND_ALONE 0x00000001
#define CONTEXT_FLAG_DISPATCHER  0x00000002

/** API Context
 */
struct RazorbackContext
{
    uuid_t uuidNuggetId;
    uuid_t uuidNuggetType;
    uuid_t uuidApplicationType;
    char *sNuggetName;
    uint32_t iFlags;
    uint32_t iDataTypeCount;
    uuid_t *pDataTypeList;
    struct RazorbackInspectionHooks *pInspectionHooks;
    struct RazorbackCommandAndControlHooks *pCommandHooks;
    sem_t regSem;
    bool regOk;
};

/** Initialize an API context.
 * @param The context to initilize
 * @return true on success false on failure.
 */
extern bool Razorback_Init_Context (struct RazorbackContext *p_pContext);

/** Initialize an Inspection API context.
 * @param p_uuidNuggetId the nugget uuid
 * @param p_uuidApplicationType the application type.
 * @param p_iDataTypeCount the number of data types.
 * @param p_pDataTypeList the list of data types.
 * #param p_pInspectionHooks the inspection call backs.
 * @return true on success false on failure.
 */
extern struct RazorbackContext * Razorback_Init_Inspection_Context (
        uuid_t p_uuidNuggetId, uuid_t p_uuidApplicationType,
        uint32_t p_iDataTypeCount, uuid_t *p_pDataTypeList,
        struct RazorbackInspectionHooks *p_pInspectionHooks);

/** Initialize a Collection API context.
 * @param p_uuidNuggetId the nugget uuid
 * @param p_uuidApplicationType the application type.
 * @return true on success false on failure.
 */
extern struct RazorbackContext * Razorback_Init_Collection_Context (
        uuid_t p_uuidNuggetId, uuid_t p_uuidApplicationType);

/** Lookup a Context by UUID.
 * @param the nugget ID uuid.
 * @return the context or NULL if there is no such context.
 */
extern struct RazorbackContext * Razorback_LookupContext (uuid_t p_uuidNugget);

extern void Razorback_Shutdown_Context (struct RazorbackContext *context);
/* Make APIs standardized while keeping function naming convention */
#define RZB_Register_Collector          Razorback_Init_Collection_Context
#define RZB_DataBlock_Create            BlockPool_CreateItem
#define RZB_DataBlock_Add_Data          BlockPool_AddData
#define RZB_DataBlock_Set_Type          BlockPool_SetItemDataType
#define RZB_DataBlock_Finalize          BlockPool_FinalizeItem
#define RZB_DataBlock_Metadata_Filename BlockPool_Metadata_Add_Filename
#define RZB_DataBlock_Submit            Submission_Submit
#define RZB_Log                         rzb_log

#endif //RAZORBACK_API_H
