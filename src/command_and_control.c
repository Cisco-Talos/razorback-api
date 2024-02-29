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

#include <signal.h>
#include <time.h>
#include <errno.h>
static bool sg_bQueueInitialized = false;
static struct Thread *sg_tThread;
static timer_t sg_tHelloTimer;
static union CcMessageUnion sg_ccmuMessage;
pthread_mutex_t sg_mPauseLock;

static void CommandAndControl_Thread (struct Thread *p_pThread);
static bool CommandAndControl_DispatchCommand (struct RazorbackContext *);
static bool CommandAndControl_Register (struct RazorbackContext *);
static bool CommandAndControl_SendHello (struct RazorbackContext *p_pContext);
static void CommandAndControl_HelloTimer (union sigval val);
static bool CommandAndControl_ArmHelloTimer (void);
static bool CommandAndControl_processHelloMessage (struct MessageHello *);


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
                rzb_log (LOG_ERR, "C&C Error: Failed to connect to MQ.");
                return false;
            }
            ConnectedEntityList_Start ();

            sg_bQueueInitialized = true;
            if ((sg_tThread =
                 Thread_Launch (CommandAndControl_Thread, NULL,
                                (char *)"Command and Control", p_pContext)) == NULL)
            {
                rzb_log (LOG_ERR, "C&C Error: Failed to launch C&C thread.");
                return false;
            }

        }
    }
    else
    {
        if (!sg_bQueueInitialized)
        {
            rzb_log (LOG_ERR,
                     "C&C Error: Can't start child context without a "
                     "running master context");
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
CommandAndControl_Thread (struct Thread *p_pContext)
{
    // local variables for command processing
    struct RazorbackContext *l_pContext;
    while (true)
    {
        if (!CommandQueue_Get (&sg_ccmuMessage))
        {
            // timeout
            if (errno == EAGAIN)
                continue;
            // error
            rzb_log (LOG_ERR,
                     "Dropped command due to failure of CommandQueue_Get()");
            // drop message
            continue;
        };
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
        rzb_log (LOG_ERR, "Command Dropped: C&C Hooks NULL");
        return false;
    }
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG,
             "C&C Message Dispatch: Type: 0x%08x Length: 0x%08x Version: 0x%08x",
             sg_ccmuMessage.mchHeader.mhHeader.iType,
             sg_ccmuMessage.mchHeader.mhHeader.iLength,
             sg_ccmuMessage.mchHeader.mhHeader.iVersion);

    if (rzb_get_log_level () == LOG_DEBUG)
    {
        uuid_unparse (sg_ccmuMessage.mchHeader.uuidSourceNugget,
                      l_sUuidSource);
        uuid_unparse (sg_ccmuMessage.mchHeader.uuidDestNugget, l_sUuidDest);
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Source %s Dest: %s",
                 l_sUuidSource, l_sUuidDest);
    }
#endif //CNC_DEBUG

    switch (sg_ccmuMessage.mchHeader.mhHeader.iType)
    {
    case MESSAGE_TYPE_HELLO:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: ==== Start Hello Message =====");
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Nugget Type UUID: %s Desc: %s",
                sg_ccmuMessage.mhHello.uuidNuggetType, UUID_TYPE_NUGGET_TYPE);
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Application Type UUID: %s Desc: %s",
                sg_ccmuMessage.mhHello.uuidApplicationType, UUID_TYPE_NUGGET);
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: ==== End Hello Message ====");
#endif //CNC_DEBUG
        return CommandAndControl_processHelloMessage (&sg_ccmuMessage.
                                                      mhHello);
        break;
    case MESSAGE_TYPE_REG_REQ:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG,
                 "C&C Msg Dispatch: ==== Start Registration Request Message ====");
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Nugget Type UUID: %s Desc: %s",
                sg_ccmuMessage.mrrRegReq.uuidNuggetType, UUID_TYPE_NUGGET_TYPE);
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Application Type UUID: %s Desc: %s",
                sg_ccmuMessage.mrrRegReq.uuidApplicationType, UUID_TYPE_NUGGET_TYPE);
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Data Type Count: %i",
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
                 "C&C Msg Dispatch: ==== End Registration Request Message ====");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRegReqMessage != NULL)
            return p_pContext->
                pCommandHooks->processRegReqMessage (&sg_ccmuMessage.
                                                     mrrRegReq);
        break;
    case MESSAGE_TYPE_REG_RESP:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG,
                 "C&C Msg Dispatch: Registration Response Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRegRespMessage != NULL)
            return p_pContext->
                pCommandHooks->processRegRespMessage (&sg_ccmuMessage.
                                                      mrrRegResp);
        break;
    case MESSAGE_TYPE_REG_ERR:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Registration Error Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRegErrMessage != NULL)
            return p_pContext->
                pCommandHooks->processRegErrMessage (&sg_ccmuMessage.meError);
        break;
    case MESSAGE_TYPE_CONFIG_UPDATE:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Configuration Update Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processConfUpdateMessage != NULL)
            return p_pContext->
                pCommandHooks->processConfUpdateMessage (&sg_ccmuMessage.
                                                         mcuConfigUpdate);
        break;
    case MESSAGE_TYPE_CONFIG_ACK:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Configuration Ack Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processConfAckMessage != NULL)
            return p_pContext->
                pCommandHooks->processConfAckMessage (&sg_ccmuMessage.
                                                      mcaConfigAck);
        break;
    case MESSAGE_TYPE_CONFIG_ERR:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Configuration Error Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processConfErrMessage != NULL)
            return p_pContext->
                pCommandHooks->processConfErrMessage (&sg_ccmuMessage.
                                                      meError);
        break;
    case MESSAGE_TYPE_STATS_REQ:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Stats Request Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processStatsReqMessage != NULL)
            return p_pContext->
                pCommandHooks->processStatsReqMessage (&sg_ccmuMessage.
                                                       msrStatsReq);
        break;
    case MESSAGE_TYPE_STATS_RESP:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Stats Response Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processStatsRespMessage != NULL)
            return p_pContext->
                pCommandHooks->processStatsRespMessage (&sg_ccmuMessage.
                                                        msrStatsResp);
        break;
    case MESSAGE_TYPE_STATS_ERR:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Stats Error Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processStatsErrMessage != NULL)
            return p_pContext->
                pCommandHooks->processStatsErrMessage (&sg_ccmuMessage.
                                                       meError);
        break;
    case MESSAGE_TYPE_PAUSE:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Pause Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processPauseMessage != NULL)
            return p_pContext->
                pCommandHooks->processPauseMessage (&sg_ccmuMessage.mpPause);
        break;
    case MESSAGE_TYPE_PAUSED:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Paused Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processPausedMessage != NULL)
            return p_pContext->
                pCommandHooks->processPausedMessage (&sg_ccmuMessage.
                                                     mpPaused);
        break;
    case MESSAGE_TYPE_GO:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Go Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processGoMessage != NULL)
            return p_pContext->
                pCommandHooks->processGoMessage (&sg_ccmuMessage.mgGo);
        break;
    case MESSAGE_TYPE_RUNNING:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Running Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRunningMessage != NULL)
            return p_pContext->
                pCommandHooks->processRunningMessage (&sg_ccmuMessage.
                                                      mrRunning);
        break;
    case MESSAGE_TYPE_TERM:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Terminate Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processTermMessage != NULL)
            return p_pContext->
                pCommandHooks->processTermMessage (&sg_ccmuMessage.
                                                   mtTerminate);
        break;
    case MESSAGE_TYPE_BYE:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "C&C Msg Dispatch: Bye Message");
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processByeMessage != NULL)
            return p_pContext->
                pCommandHooks->processByeMessage (&sg_ccmuMessage.mbBye);
        break;
    default:
        rzb_log (LOG_ERR,
                 "Dropped C&C Message: Bad Type (%i)",
                 sg_ccmuMessage.mchHeader.mhHeader.iType);
        return false;
    }
    return true;
}


static bool
CommandAndControl_Register (struct RazorbackContext *p_pContext)
{
    struct MessageRegistrationRequest l_mrrRegReq;
    pthread_mutex_lock(&sg_mPauseLock); // Lock the pause Lock
    if (!MessageRegistrationRequest_Initialize (&l_mrrRegReq,
                                                p_pContext->uuidNuggetId,
                                                p_pContext->uuidNuggetType,
                                                p_pContext->uuidApplicationType,
                                                p_pContext->iDataTypeCount,
                                                p_pContext->pDataTypeList))
    {
        rzb_log (LOG_ERR,
                 "C&C Register: Failed to Init Registration Request");
        return false;
    }

    if (!CommandQueue_Put ((union CcMessageUnion *) &l_mrrRegReq))
    {
        rzb_log (LOG_ERR,
                 "C&C Register: Failed to send registration Request");
        return false;
    }


    return true;
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
        rzb_log (LOG_ERR, "C&C Hello Timer: Failed to send Hello Message");
        return false;
    }

    return true;
}

static void
CommandAndControl_HelloTimer (union sigval val)
{
    Razorback_ForEach_Context (CommandAndControl_SendHello);
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
                 "C&C Arm Hello Timer: Failed to malloc timer properties");
        return false;
    }
    l_pProps->sigev_notify = SIGEV_THREAD;
    l_pProps->sigev_value.sival_ptr = NULL;
    l_pProps->sigev_notify_function = &CommandAndControl_HelloTimer;
    if (timer_create (CLOCK_REALTIME, l_pProps, &sg_tHelloTimer) == -1)
    {
        rzb_log (LOG_ERR, "C&C Arm Hello Timer: Failed call to timer_create");
        return false;
    }
    if (timer_settime (sg_tHelloTimer, 0, &l_itsTimerSpec, NULL) == -1)
    {
        rzb_log (LOG_ERR, "C&C Arm Hello Timer: Failed to arm timer.\n");
        return false;
    }
    return true;
}

static bool
CommandAndControl_processHelloMessage (struct MessageHello *mhHello)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Hook: Hello");
#endif //CNC_DEBUG
    struct RazorbackContext *l_pContext = NULL;
    uint32_t l_iState =0;
    struct MessageTerminate l_mtTerm;

    l_pContext = Thread_GetContext(sg_tThread);

    if ((l_pContext->iFlags & CONTEXT_FLAG_DISPATCHER) ==
        CONTEXT_FLAG_DISPATCHER)
    {
        ConnectedEntityList_GetState (mhHello->ccmhHeader.uuidSourceNugget,
                                    mhHello->uuidNuggetType,
                                    mhHello->uuidApplicationType, &l_iState);
        if (l_iState == 0) 
        {
            // TODO: This should be a re-register message not terminate.
            MessageTerminate_Initialize(&l_mtTerm, 
                mhHello->ccmhHeader.uuidDestNugget,
                mhHello->ccmhHeader.uuidSourceNugget,
                (uint8_t *)"Not Registered");
            if (!CommandQueue_Put ((union CcMessageUnion *) &l_mtTerm))
            {
                rzb_log (LOG_ERR, "C&C Hello Processor: Failed to send Terminate Message");
                return false;
            }

            MessageTerminate_Destroy(&l_mtTerm);
            return true;
        }

    }
    ConnectedEntityList_Update (mhHello->ccmhHeader.uuidSourceNugget,
                                mhHello->uuidNuggetType,
                                mhHello->uuidApplicationType);
    return true;
}

static bool
Default_processRegReqMessage (struct MessageRegistrationRequest *mrrRegReq)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Reg Req");
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processRegRespMessage (struct MessageRegistrationResponse *mrrRegResp)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Reg Resp");
#endif //CNC_DEBUG
    struct RazorbackContext *l_pContext;

    if ((l_pContext =
         Razorback_LookupContext (mrrRegResp->ccmhMessageHeader.
                                  uuidDestNugget)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "C&C Registration Response: Failed to get valid context for message");
    }

    if ((l_pContext->iFlags & CONTEXT_FLAG_STAND_ALONE) ==
        CONTEXT_FLAG_STAND_ALONE)
    {
        CommandAndControl_ArmHelloTimer ();
    }

    return true;
}

static bool
Default_processRegErrMessage (struct MessageError *meError)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Reg Err");
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processConfUpdateMessage (struct MessageConfigurationUpdate
                                  *mcuConfigUpdate)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Conf Update");
#endif //CNC_DEBUG
    struct RazorbackContext *l_pContext;
    struct MessageConfigurationAck l_mcaConfigAck;

    if ((l_pContext =
         Razorback_LookupContext (mcuConfigUpdate->ccmhMessageHeader.
                                  uuidDestNugget)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "C&C Config Update: Failed to get valid context for message");
    }
    MessageConfigurationAck_Initialize(&l_mcaConfigAck,
                             l_pContext->uuidNuggetId,
                             mcuConfigUpdate->ccmhMessageHeader.uuidSourceNugget,
                             l_pContext->uuidNuggetType,
                             l_pContext->uuidApplicationType);

    if (!CommandQueue_Put ((union CcMessageUnion *) &l_mcaConfigAck))
    {
        rzb_log (LOG_ERR, "C&C Hello Timer: Failed to send Configuration Ack Message");
        return false;
    }

    return true;
}

static bool
Default_processConfAckMessage (struct MessageConfigurationAck *mcaConfigAck)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Conf Ack");
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processConfErrMessage (struct MessageError *meError)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Conf Err");
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processStatsReqMessage (struct MessageStatsRequest *msrStatsReq)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Stats Req");
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processStatsRespMessage (struct MessageStatsResponse *msrStatsResp)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Stats Resp");
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processStatsErrMessage (struct MessageError *meError)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Stats Err");
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processPauseMessage (struct MessagePause *mpPause)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Pause");
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processPausedMessage (struct MessagePaused *mpPaused)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Paused");
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processGoMessage (struct MessageGo *mgGo)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Go");
#endif //CNC_DEBUG
    struct MessageRunning l_mrRunning;
    pthread_mutex_unlock(&sg_mPauseLock);
    MessageRunning_Initialize(&l_mrRunning,
            mgGo->ccmhMessageHeader.uuidDestNugget,
            mgGo->ccmhMessageHeader.uuidSourceNugget);
    if (!CommandQueue_Put ((union CcMessageUnion *) &l_mrRunning))
    {
        rzb_log (LOG_ERR, "Go Hook: Failed to send running message");
        return false;
    }
//    MessageRunning_Destroy(&l_mrRunning);
    return true;
}

static bool
Default_processRunningMessage (struct MessageRunning *mrRunning)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Running");
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processTermMessage (struct MessageTerminate *mtTerminate)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Terminate");
#endif //CNC_DEBUG
    struct MessageBye l_mbBye;
    MessageBye_Initialize(&l_mbBye, 
            mtTerminate->ccmhMessageHeader.uuidDestNugget,
            mtTerminate->ccmhMessageHeader.uuidSourceNugget);

    if (!CommandQueue_Put ((union CcMessageUnion *) &l_mbBye))
    {
        rzb_log (LOG_ERR, "Terminate Hook: Failed to send bye message");
    }
    exit(0); // TODO: This should be a clean shutdown
    return true;
}

static bool
Default_processByeMessage (struct MessageBye *mbBye)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "C&C Default Hook: Bye");
#endif //CNC_DEBUG
    return true;
}
