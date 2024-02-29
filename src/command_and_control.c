#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/command_queue.h>
#include <razorback/connected_entity.h>
#include <razorback/ntlv.h>
#include <razorback/thread.h>
#include <razorback/log.h>
#include <razorback/uuids.h>
#include <razorback/api.h>
#include "command_and_control.h"
#include "runtime_config.h"
#include "api_internal.h"
#include "connected_entity_private.h"
#include "local_cache.h"

#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
static bool sg_bQueueInitialized = false;
static struct Thread *sg_tThread;
static timer_t sg_tHelloTimer;
static union CcMessageUnion sg_ccmuMessage;
pthread_mutex_t sg_mPauseLock;
pthread_mutex_t processLock = PTHREAD_MUTEX_INITIALIZER;

static void CommandAndControl_Thread (struct Thread *p_pThread);
static bool CommandAndControl_DispatchCommand (struct RazorbackContext *);
static bool CommandAndControl_Register (struct RazorbackContext *);
static bool CommandAndControl_SendHello (struct RazorbackContext *p_pContext);
static void CommandAndControl_HelloTimer (union sigval val);
static bool CommandAndControl_ArmHelloTimer (void);
static bool CommandAndControl_processHelloMessage (struct MessageHello *);
static bool CommandAndControl_processCacheClearMessage (struct RazorbackContext *context);


static bool Default_processRegReqMessage (struct MessageRegistrationRequest
                                          *);
static bool Default_processRegRespMessage (struct MessageRegistrationResponse
                                           *);
static bool Default_processRegErrMessage (struct MessageError *);
static bool Default_processConfUpdateMessage (struct
                                              MessageConfigurationUpdate *);
static bool Default_processConfAckMessage (struct MessageConfigurationAck *);
static bool Default_processConfErrMessage (struct MessageError *);
static bool Default_processStatsReqMessage (struct MessageStatsRequest *);
static bool Default_processStatsRespMessage (struct MessageStatsResponse *);
static bool Default_processStatsErrMessage (struct MessageError *);
static bool Default_processPauseMessage (struct MessagePause *);
static bool Default_processPausedMessage (struct MessagePaused *);
static bool Default_processGoMessage (struct MessageGo *);
static bool Default_processRunningMessage (struct MessageRunning *);
static bool Default_processTermMessage (struct MessageTerminate *);
static bool Default_processByeMessage (struct MessageBye *);

static struct RazorbackCommandAndControlHooks sg_DefaultHooks = {
    Default_processRegReqMessage,
    Default_processRegRespMessage,
    Default_processRegErrMessage,
    Default_processConfUpdateMessage,
    Default_processConfAckMessage,
    Default_processConfErrMessage,
    Default_processStatsReqMessage,
    Default_processStatsRespMessage,
    Default_processStatsErrMessage,
    Default_processPauseMessage,
    Default_processPausedMessage,
    Default_processGoMessage,
    Default_processRunningMessage,
    Default_processTermMessage,
    Default_processByeMessage
};


void
CommandAndControl_Pause(void)
{
    pthread_mutex_lock(&processLock);
}
void
CommandAndControl_Unpause(void)
{
    pthread_mutex_unlock(&processLock);
}

SO_PUBLIC bool
CommandAndControl_Start (struct RazorbackContext *p_pContext)
{
    if (p_pContext->pCommandHooks == NULL)
        p_pContext->pCommandHooks = &sg_DefaultHooks;


    if ((p_pContext->iFlags & CONTEXT_FLAG_STAND_ALONE) ==
        CONTEXT_FLAG_STAND_ALONE)
    {
        if (!sg_bQueueInitialized)
        {
            pthread_mutex_init(&sg_mPauseLock, NULL);
            if (!CommandQueue_Initialize ())
            {
                rzb_log (LOG_ERR, "%s: C&C Error: Failed to connect to MQ.", __func__);
                return false;
            }
            ConnectedEntityList_Start ();

            sg_bQueueInitialized = true;
            if ((sg_tThread =
                 Thread_Launch (CommandAndControl_Thread, NULL,
                                (char *)"Command and Control", p_pContext)) == NULL)
            {
                rzb_log (LOG_ERR, "%s: C&C Error: Failed to launch C&C thread.", __func__);
                return false;
            }

        }
    }
    else
    {
        if (!sg_bQueueInitialized)
        {
            rzb_log (LOG_ERR,
                     "%s: C&C Error: Can't start child context without a "
                     "running master context", __func__);
            return false;
        }

    }
    // Kick Start Registration
    if ((p_pContext->iFlags & CONTEXT_FLAG_DISPATCHER) ==
        CONTEXT_FLAG_DISPATCHER)
        return CommandAndControl_ArmHelloTimer ();
    else
        return CommandAndControl_Register (p_pContext);
}


static void 
CommandAndControl_Thread (struct Thread *p_pThread)
{
    // local variables for command processing
    struct RazorbackContext *l_pContext;
    while (!Thread_IsStopped(p_pThread))
    {
        memset(&sg_ccmuMessage, 0, sizeof (union CcMessageUnion));
        if (!CommandQueue_Get (&sg_ccmuMessage))
        {
            // timeout
            if (errno == EAGAIN)
                continue;
            // error
            rzb_log (LOG_ERR,
                     "%s: Dropped command due to failure of CommandQueue_Get()", __func__);
            // drop message
            continue;
        }
        pthread_mutex_lock(&processLock);
        if (uuid_is_null (sg_ccmuMessage.mchHeader.uuidDestNugget) == 1)    // Broadcast Message
        {
            if ((l_pContext =
                 Razorback_LookupContext (sg_ccmuMessage.
                                          mchHeader.uuidSourceNugget)) == NULL) // If its not from one of my context's
                Razorback_ForEach_Context (CommandAndControl_DispatchCommand);
        }
        else                    // Directed Message
        {
            if ((l_pContext =
                 Razorback_LookupContext (sg_ccmuMessage.
                                          mchHeader.uuidDestNugget)) != NULL)
                CommandAndControl_DispatchCommand (l_pContext);
        }
        pthread_mutex_unlock(&processLock);
        MessageCC_Destroy (&sg_ccmuMessage);
    }
    return;
}

#ifdef CNC_DEBUG
static void 
CommandAndControl_PrintUuid(int p_iLevel, const char *p_fmt, uuid_t p_uuid, uint32_t p_iUuidType)
{
    char *l_sUuidDesc;
    char l_sUuid[UUID_STRING_LENGTH];
    uuid_unparse (p_uuid, l_sUuid);
    l_sUuidDesc =
        UUID_Get_DescriptionByUUID (p_uuid, p_iUuidType);
    rzb_log (p_iLevel, p_fmt, l_sUuid, l_sUuidDesc);
    free(l_sUuidDesc);
}
#endif //CNC_DEBUG

static bool
CommandAndControl_DispatchCommand (struct RazorbackContext *p_pContext)
{
    ASSERT (p_pContext->pCommandHooks != NULL);

#ifdef CNC_DEBUG
    char l_sUuidSource[UUID_STRING_LENGTH], l_sUuidDest[UUID_STRING_LENGTH];
    uint32_t l_iDataTypeIttr;
#endif //CNC_DEBUG

    if (p_pContext->pCommandHooks == NULL)
    {
        rzb_log (LOG_ERR, "%s: Command Dropped: C&C Hooks NULL", __func__);
        return false;
    }
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG,
             "%s: C&C Message Dispatch: Type: 0x%08x Length: 0x%08x Version: 0x%08x", __func__, 
             sg_ccmuMessage.mchHeader.mhHeader.iType,
             sg_ccmuMessage.mchHeader.mhHeader.iLength,
             sg_ccmuMessage.mchHeader.mhHeader.iVersion);

    if (rzb_get_log_level () == LOG_DEBUG)
    {
        uuid_unparse (sg_ccmuMessage.mchHeader.uuidSourceNugget,
                      l_sUuidSource);
        uuid_unparse (sg_ccmuMessage.mchHeader.uuidDestNugget, l_sUuidDest);
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Source %s Dest: %s", __func__,
                 l_sUuidSource, l_sUuidDest);
    }
#endif //CNC_DEBUG

    switch (sg_ccmuMessage.mchHeader.mhHeader.iType)
    {
    case MESSAGE_TYPE_HELLO:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: ==== Start Hello Message =====", __func__);
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Nugget Type UUID: %s Desc: %s",
                sg_ccmuMessage.mhHello.uuidNuggetType, UUID_TYPE_NUGGET_TYPE);
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Application Type UUID: %s Desc: %s",
                sg_ccmuMessage.mhHello.uuidApplicationType, UUID_TYPE_NUGGET);
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: ==== End Hello Message ====", __func__);
#endif //CNC_DEBUG
        return CommandAndControl_processHelloMessage (&sg_ccmuMessage.
                                                      mhHello);
        break;
    case MESSAGE_TYPE_REG_REQ:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG,
                 "%s: C&C Msg Dispatch: ==== Start Registration Request Message ====", __func__);
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Nugget Type UUID: %s Desc: %s",
                sg_ccmuMessage.mrrRegReq.uuidNuggetType, UUID_TYPE_NUGGET_TYPE);
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Application Type UUID: %s Desc: %s",
                sg_ccmuMessage.mrrRegReq.uuidApplicationType, UUID_TYPE_NUGGET_TYPE);
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Data Type Count: %i", __func__,
                 sg_ccmuMessage.mrrRegReq.iDataTypeCount);
        for (l_iDataTypeIttr = 0;
             l_iDataTypeIttr < sg_ccmuMessage.mrrRegReq.iDataTypeCount;
             l_iDataTypeIttr++)
        {
            CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Data Type UUID: %s Desc: %s",
                                            sg_ccmuMessage.mrrRegReq.
                                            pDataTypeList[l_iDataTypeIttr],
                                            UUID_TYPE_DATA_TYPE);
        }
        rzb_log (LOG_DEBUG,
                 "%s: C&C Msg Dispatch: ==== End Registration Request Message ====", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRegReqMessage != NULL)
            return p_pContext->
                pCommandHooks->processRegReqMessage (&sg_ccmuMessage.
                                                     mrrRegReq);
        break;
    case MESSAGE_TYPE_REG_RESP:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG,
                 "%s: C&C Msg Dispatch: Registration Response Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRegRespMessage != NULL)
            return p_pContext->
                pCommandHooks->processRegRespMessage (&sg_ccmuMessage.
                                                      mrrRegResp);
        break;
    case MESSAGE_TYPE_REG_ERR:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Registration Error Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRegErrMessage != NULL)
            return p_pContext->
                pCommandHooks->processRegErrMessage (&sg_ccmuMessage.meError);
        break;
    case MESSAGE_TYPE_CONFIG_UPDATE:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Configuration Update Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processConfUpdateMessage != NULL)
            return p_pContext->
                pCommandHooks->processConfUpdateMessage (&sg_ccmuMessage.
                                                         mcuConfigUpdate);
        break;
    case MESSAGE_TYPE_CONFIG_ACK:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Configuration Ack Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processConfAckMessage != NULL)
            return p_pContext->
                pCommandHooks->processConfAckMessage (&sg_ccmuMessage.
                                                      mcaConfigAck);
        break;
    case MESSAGE_TYPE_CONFIG_ERR:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Configuration Error Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processConfErrMessage != NULL)
            return p_pContext->
                pCommandHooks->processConfErrMessage (&sg_ccmuMessage.
                                                      meError);
        break;
    case MESSAGE_TYPE_STATS_REQ:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Stats Request Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processStatsReqMessage != NULL)
            return p_pContext->
                pCommandHooks->processStatsReqMessage (&sg_ccmuMessage.
                                                       msrStatsReq);
        break;
    case MESSAGE_TYPE_STATS_RESP:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Stats Response Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processStatsRespMessage != NULL)
            return p_pContext->
                pCommandHooks->processStatsRespMessage (&sg_ccmuMessage.
                                                        msrStatsResp);
        break;
    case MESSAGE_TYPE_STATS_ERR:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Stats Error Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processStatsErrMessage != NULL)
            return p_pContext->
                pCommandHooks->processStatsErrMessage (&sg_ccmuMessage.
                                                       meError);
        break;
    case MESSAGE_TYPE_PAUSE:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Pause Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processPauseMessage != NULL)
            return p_pContext->
                pCommandHooks->processPauseMessage (&sg_ccmuMessage.mpPause);
        break;
    case MESSAGE_TYPE_PAUSED:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Paused Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processPausedMessage != NULL)
            return p_pContext->
                pCommandHooks->processPausedMessage (&sg_ccmuMessage.
                                                     mpPaused);
        break;
    case MESSAGE_TYPE_GO:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Go Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processGoMessage != NULL)
            return p_pContext->
                pCommandHooks->processGoMessage (&sg_ccmuMessage.mgGo);
        break;
    case MESSAGE_TYPE_RUNNING:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Running Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRunningMessage != NULL)
            return p_pContext->
                pCommandHooks->processRunningMessage (&sg_ccmuMessage.
                                                      mrRunning);
        break;
    case MESSAGE_TYPE_TERM:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Terminate Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processTermMessage != NULL)
            return p_pContext->
                pCommandHooks->processTermMessage (&sg_ccmuMessage.
                                                   mtTerminate);
        break;
    case MESSAGE_TYPE_BYE:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Bye Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processByeMessage != NULL)
            return p_pContext->
                pCommandHooks->processByeMessage (&sg_ccmuMessage.mbBye);
        break;
    case MESSAGE_TYPE_CLEAR:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Cache Clear Message", __func__);
#endif //CNC_DEBUG
        return CommandAndControl_processCacheClearMessage (p_pContext);
        break;
    default:
        rzb_log (LOG_ERR,
                 "%s: Dropped C&C Message: Bad Type (%i)", __func__, 
                 sg_ccmuMessage.mchHeader.mhHeader.iType);
        return false;
    }
    return true;
}

bool 
CommandAndControl_SendBye (struct RazorbackContext *context)
{
    struct MessageBye bye;
    uuid_t bCast;
    uuid_clear(bCast);
    MessageBye_Initialize(&bye, 
            context->uuidNuggetId, bCast);

    if (!CommandQueue_Put ((union CcMessageUnion *) &bye))
    {
        rzb_log (LOG_ERR, "%s: Failed to send bye message", __func__);
        return false;
    }
    return true;
}

static bool
CommandAndControl_Register (struct RazorbackContext *p_pContext)
{
    struct MessageRegistrationRequest l_mrrRegReq;

    if (!MessageRegistrationRequest_Initialize (&l_mrrRegReq,
                                                p_pContext->uuidNuggetId,
                                                p_pContext->uuidNuggetType,
                                                p_pContext->uuidApplicationType,
                                                p_pContext->iDataTypeCount,
                                                p_pContext->pDataTypeList))
    {
        rzb_log (LOG_ERR,
                 "%s: C&C Register: Failed to Init Registration Request", __func__);
        return false;
    }

    if (!CommandQueue_Put ((union CcMessageUnion *) &l_mrrRegReq))
    {
        rzb_log (LOG_ERR,
                 "%s: C&C Register: Failed to send registration Request", __func__);
        return false;
    }
    MessageRegistrationRequest_Destroy (&l_mrrRegReq);
    // Wait for Go Message
    sem_wait(&p_pContext->regSem);
    return p_pContext->regOk;
}

static bool
CommandAndControl_SendHello (struct RazorbackContext *p_pContext)
{
    struct MessageHello l_mhHello;
    MessageHello_Initialize (&l_mhHello,
                             p_pContext->uuidNuggetId,
                             p_pContext->uuidNuggetType,
                             p_pContext->uuidApplicationType);
    if (!CommandQueue_Put ((union CcMessageUnion *) &l_mhHello))
    {
        rzb_log (LOG_ERR, "%s: C&C Hello Timer: Failed to send Hello Message", __func__);
        return false;
    }

    return true;
}

static void
CommandAndControl_HelloTimer (union sigval val)
{
    CommandAndControl_Pause();
    Razorback_ForEach_Context (CommandAndControl_SendHello);
    CommandAndControl_Unpause();
}

static bool
CommandAndControl_ArmHelloTimer (void)
{
    struct sigevent *l_pProps;
    struct itimerspec l_itsTimerSpec;
    l_itsTimerSpec.it_value.tv_sec = Config_getHelloTime ();
    l_itsTimerSpec.it_value.tv_nsec = 1;
    l_itsTimerSpec.it_interval.tv_sec = Config_getHelloTime ();
    l_itsTimerSpec.it_interval.tv_nsec = 1;
    if ((l_pProps = calloc (1, sizeof (struct sigevent))) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: C&C Arm Hello Timer: Failed to malloc timer properties", __func__);
        return false;
    }
    l_pProps->sigev_notify = SIGEV_THREAD;
    l_pProps->sigev_value.sival_ptr = NULL;
    l_pProps->sigev_notify_function = &CommandAndControl_HelloTimer;
    if (timer_create (CLOCK_REALTIME, l_pProps, &sg_tHelloTimer) == -1)
    {
        rzb_log (LOG_ERR, "%s: C&C Arm Hello Timer: Failed call to timer_create", __func__);
        free(l_pProps);
        return false;
    }
    if (timer_settime (sg_tHelloTimer, 0, &l_itsTimerSpec, NULL) == -1)
    {
        rzb_log (LOG_ERR, "%s: C&C Arm Hello Timer: Failed to arm timer.", __func__);
        free(l_pProps);
        return false;
    }
    free(l_pProps);
    return true;
}

static bool
CommandAndControl_processHelloMessage (struct MessageHello *mhHello)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Hook: Hello", __func__);
#endif //CNC_DEBUG
    struct RazorbackContext *l_pContext = NULL;
    uint32_t l_iState =0;
    struct MessageTerminate l_mtTerm;
    char *l_sNugType, *l_sAppType, l_sNugId[UUID_STRING_LENGTH];

    l_pContext = Thread_GetContext(sg_tThread);

    if ((l_pContext->iFlags & CONTEXT_FLAG_DISPATCHER) ==
        CONTEXT_FLAG_DISPATCHER)
    {
        ConnectedEntityList_GetState (mhHello->ccmhHeader.uuidSourceNugget, &l_iState);
        if (l_iState == 0) 
        {
            l_sNugType = UUID_Get_NameByUUID(mhHello->uuidNuggetType, UUID_TYPE_NUGGET_TYPE);
            l_sAppType = UUID_Get_NameByUUID(mhHello->uuidApplicationType, UUID_TYPE_NUGGET);
            uuid_unparse(mhHello->ccmhHeader.uuidSourceNugget, l_sNugId);
            rzb_log(LOG_INFO, "%s: Hello recieved from unregistered nugget (termination requested): %s. %s, %s", __func__, l_sNugType, l_sAppType, l_sNugId);
            free(l_sNugType);
            free(l_sAppType);
            // TODO: This should be a re-register message not terminate.
            MessageTerminate_Initialize(&l_mtTerm, 
                mhHello->ccmhHeader.uuidDestNugget,
                mhHello->ccmhHeader.uuidSourceNugget,
                (uint8_t *)"Not Registered");
            if (!CommandQueue_Put ((union CcMessageUnion *) &l_mtTerm))
            {
                rzb_log (LOG_ERR, "%s: C&C Hello Processor: Failed to send Terminate Message", __func__);
                return false;
            }

            MessageTerminate_Destroy(&l_mtTerm);
            return true;
        }
        // TODO: Track number of dispatchers online
        ConnectedEntityList_Update (mhHello->ccmhHeader.uuidSourceNugget);
    }
    return true;
}


static bool
CommandAndControl_processCacheClearMessage (struct RazorbackContext *context)
{
    
    // If we are not a dispatcher but we are stand alone then process this request.
    if (
            ((context->iFlags & CONTEXT_FLAG_DISPATCHER) == 0) &&
            ((context->iFlags & CONTEXT_FLAG_STAND_ALONE) == CONTEXT_FLAG_STAND_ALONE)
       )
    {
        rzb_log(LOG_INFO, "%s: Clearing Local Cache", __func__);
        clearLocalEntry(ALL, FULL);
    }
    return true;
}


static bool
Default_processRegReqMessage (struct MessageRegistrationRequest *mrrRegReq)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Reg Req", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processRegRespMessage (struct MessageRegistrationResponse *mrrRegResp)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Reg Resp", __func__);
#endif //CNC_DEBUG
    struct RazorbackContext *l_pContext;

    if ((l_pContext =
         Razorback_LookupContext (mrrRegResp->ccmhMessageHeader.
                                  uuidDestNugget)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: C&C Registration Response: Failed to get valid context for message", __func__);
    }

    if ((l_pContext->iFlags & CONTEXT_FLAG_STAND_ALONE) ==
        CONTEXT_FLAG_STAND_ALONE)
    {
        CommandAndControl_SendHello(l_pContext);
        CommandAndControl_ArmHelloTimer ();
    }

    return true;
}

static bool
Default_processRegErrMessage (struct MessageError *meError)
{
    struct RazorbackContext *l_pContext;
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Reg Err", __func__);
#endif //CNC_DEBUG
    rzb_log(LOG_ERR, "%s: Registration Error: %s", __func__, meError->sMessage);

    if ((l_pContext = Razorback_LookupContext(meError->ccmhHeader.uuidDestNugget)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Recieved registration error message for unknown context", __func__);
        return false;
    }
    // Wait up the registering thread.
    l_pContext->regOk = false;
    sem_post(&l_pContext->regSem);
    return true;
}

static bool
Default_processConfUpdateMessage (struct MessageConfigurationUpdate
                                  *mcuConfigUpdate)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Conf Update", __func__);
#endif //CNC_DEBUG
    struct RazorbackContext *l_pContext;
    struct MessageConfigurationAck l_mcaConfigAck;
    struct UUIDList *list;
    uint32_t pos = 0;
    char *curStr;
    uuid_t *curUuid;
    
    list = UUID_Get_List(UUID_TYPE_NTLV_TYPE);
    pthread_mutex_lock(&list->mutex);
    UUID_Clear_List(list);
    curStr = mcuConfigUpdate->ntlvTypesNames;
    curUuid = mcuConfigUpdate->ntlvTypesUuids;
    for (pos =0; pos < mcuConfigUpdate->ntlvTypesCount; pos++)
    {
        UUID_Add_List_Entry(list, *curUuid, curStr, "");
        curStr = curStr + strlen(curStr) +1;
        curUuid+=1;
    }
    pthread_mutex_unlock(&list->mutex);
    list = UUID_Get_List(UUID_TYPE_NTLV_NAME);
    pthread_mutex_lock(&list->mutex);
    UUID_Clear_List(list);
    curStr = mcuConfigUpdate->ntlvNamesNames;
    curUuid = mcuConfigUpdate->ntlvNamesUuids;
    for (pos =0; pos < mcuConfigUpdate->ntlvNamesCount; pos++)
    {
        UUID_Add_List_Entry(list, *curUuid, curStr, "");
        curStr = curStr + strlen(curStr) +1;
        curUuid+=1;
    }
    pthread_mutex_unlock(&list->mutex);
    

    if ((l_pContext =
         Razorback_LookupContext (mcuConfigUpdate->ccmhMessageHeader.
                                  uuidDestNugget)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: C&C Config Update: Failed to get valid context for message", __func__);
    }
    MessageConfigurationAck_Initialize(&l_mcaConfigAck,
                             l_pContext->uuidNuggetId,
                             mcuConfigUpdate->ccmhMessageHeader.uuidSourceNugget,
                             l_pContext->uuidNuggetType,
                             l_pContext->uuidApplicationType);

    if (!CommandQueue_Put ((union CcMessageUnion *) &l_mcaConfigAck))
    {
        rzb_log (LOG_ERR, "%s: C&C Config Update: Failed to send Configuration Ack Message", __func__);
        return false;
    }

    return true;
}

static bool
Default_processConfAckMessage (struct MessageConfigurationAck *mcaConfigAck)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Conf Ack", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processConfErrMessage (struct MessageError *meError)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Conf Err", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processStatsReqMessage (struct MessageStatsRequest *msrStatsReq)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Stats Req", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processStatsRespMessage (struct MessageStatsResponse *msrStatsResp)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Stats Resp", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processStatsErrMessage (struct MessageError *meError)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Stats Err", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processPauseMessage (struct MessagePause *mpPause)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Pause", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processPausedMessage (struct MessagePaused *mpPaused)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Paused", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processGoMessage (struct MessageGo *mgGo)
{
    struct RazorbackContext *l_pContext;
    struct MessageRunning l_mrRunning;
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Go", __func__);
#endif //CNC_DEBUG

    if ((l_pContext = Razorback_LookupContext(mgGo->ccmhMessageHeader.uuidDestNugget)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Recieved go message for unknown context", __func__);
        return false;
    }
    
    // Wait up the registering thread.
    l_pContext->regOk = true;
    sem_post(&l_pContext->regSem);

    MessageRunning_Initialize(&l_mrRunning,
            mgGo->ccmhMessageHeader.uuidDestNugget,
            mgGo->ccmhMessageHeader.uuidSourceNugget);
    if (!CommandQueue_Put ((union CcMessageUnion *) &l_mrRunning))
    {
        rzb_log (LOG_ERR, "%s: Go Hook: Failed to send running message", __func__);
        return false;
    }
//    MessageRunning_Destroy(&l_mrRunning);
    return true;
}

static bool
Default_processRunningMessage (struct MessageRunning *mrRunning)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Running", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processTermMessage (struct MessageTerminate *mtTerminate)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Terminate", __func__);
#endif //CNC_DEBUG
    rzb_log(LOG_INFO, "%s: Termination Requested: %s", __func__, mtTerminate->sTerminateReason);
    struct MessageBye l_mbBye;
    MessageBye_Initialize(&l_mbBye, 
            mtTerminate->ccmhMessageHeader.uuidDestNugget,
            mtTerminate->ccmhMessageHeader.uuidSourceNugget);

    if (!CommandQueue_Put ((union CcMessageUnion *) &l_mbBye))
    {
        rzb_log (LOG_ERR, "%s: Terminate Hook: Failed to send bye message", __func__);
    }
    exit(0); // TODO: This should be a clean shutdown
    return true;
}

static bool
Default_processByeMessage (struct MessageBye *mbBye)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Bye", __func__);
#endif //CNC_DEBUG
    return true;
}
