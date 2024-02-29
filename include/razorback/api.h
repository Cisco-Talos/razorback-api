/** @file api.h
 * Razorback API.
 */
#ifndef RAZORBACK_API_H
#define RAZORBACK_API_H

#include <razorback/types.h>
#include <razorback/queue.h>
#include <razorback/messages.h>
#include <semaphore.h>

#define DECL_INSPECTION_FUNC(a) uint8_t a (struct Block *block, struct EventId *eventId, struct List *eventMetadata)
#define DECL_NUGGET_INIT bool initNug(void)
#define DECL_NUGGET_SHUTDOWN void shutdownNug(void)

/** Inspection Nugget Hooks
 */
struct RazorbackInspectionHooks
{
    uint8_t (*processBlock) (struct Block *, struct EventId *, struct List *);    ///< FP to inspection handler
    bool (*processDeferredList) (struct DeferredList *);            ///< FP to pending items

};

struct RazorbackCommandAndControlHooks
{
    bool (*processRegReqMessage) (struct Message *);
    bool (*processRegRespMessage) (struct Message *);
    bool (*processRegErrMessage) (struct Message *);
    bool (*processConfUpdateMessage) (struct Message *);
    bool (*processConfAckMessage) (struct Message *);
    bool (*processConfErrMessage) (struct Message *);
    bool (*processPauseMessage) (struct Message *);
    bool (*processPausedMessage) (struct Message *);
    bool (*processGoMessage) (struct Message *);
    bool (*processRunningMessage) (struct Message *);
    bool (*processTermMessage) (struct Message *);
    bool (*processByeMessage) (struct Message *);
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
    struct Thread *pInspectionThread;
	void *userData;
    struct Queue *judgmentQueue;
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
extern bool Razorback_Render_Verdict (struct Judgment *p_pJudgment);
/* Make APIs standardized while keeping function naming convention */
#define RZB_Register_Collector          Razorback_Init_Collection_Context
#define RZB_DataBlock_Create            BlockPool_CreateItem
#define RZB_DataBlock_Add_Data          BlockPool_AddData
#define RZB_DataBlock_Set_Type          BlockPool_SetItemDataType
#define RZB_DataBlock_Finalize          BlockPool_FinalizeItem
#define RZB_DataBlock_Metadata_Filename(block, filename) Metadata_Add_Filename(block->pEvent->pMetaDataList, filename)
#define RZB_DataBlock_Metadata_Hostname(block, hostname) Metadata_Add_Hostname(block->pEvent->pMetaDataList, hostname)
#define RZB_DataBlock_Metadata_URI(block, uri) Metadata_Add_URI(block->pEvent->pMetaDataList, uri)
#define RZB_DataBlock_Metadata_HttpRequest(block, request) Metadata_Add_HttpRequest(block->pEvent->pMetaDataList, request)
#define RZB_DataBlock_Metadata_HttpResponse(block, response) Metadata_Add_HttpResponse(block->pEvent->pMetaDataList, response)
#define RZB_DataBlock_Metadata_HttpResponse(block, response) Metadata_Add_HttpResponse(block->pEvent->pMetaDataList, response)
#define RZB_DataBlock_Metadata_IPv4_Source(block, address) Metadata_Add_IPv4_Source(block->pEvent->pMetaDataList, address)
#define RZB_DataBlock_Metadata_IPv4_Destination(block, address) Metadata_Add_IPv4_Destination(block->pEvent->pMetaDataList, address)
#define RZB_DataBlock_Metadata_IPv6_Source(block, address) Metadata_Add_IPv6_Source(block->pEvent->pMetaDataList, address)
#define RZB_DataBlock_Metadata_IPv6_Destination(block, address) Metadata_Add_IPv6_Destination(block->pEvent->pMetaDataList, address)

#define RZB_DataBlock_Metadata_Port_Source(block, port) Metadata_Add_Port_Source(block->pEvent->pMetaDataList, port)
#define RZB_DataBlock_Metadata_Port_Destination(block, port) Metadata_Add_Port_Destination(block->pEvent->pMetaDataList, port)


#define RZB_DataBlock_Submit            Submission_Submit
#define RZB_Log                         rzb_log

#endif //RAZORBACK_API_H
