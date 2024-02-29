#include "config.h"

#include <razorback/debug.h>
#include <razorback/messages.h>
#include <razorback/connected_entity.h>
#include <razorback/ntlv.h>
#include <razorback/thread.h>
#include <razorback/log.h>
#include <razorback/uuids.h>
#include <razorback/api.h>
#include <razorback/queue.h>
#include "command_and_control.h"
#include "runtime_config.h"
#include "api_internal.h"
#include "connected_entity_private.h"
#include "local_cache.h"
#include "messages/cnc/core.h"

#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>

static bool sg_bQueueInitialized = false;
static struct Thread *sg_tThread;
static timer_t sg_tHelloTimer;

pthread_mutex_t sg_mPauseLock;
pthread_mutex_t processLock = PTHREAD_MUTEX_INITIALIZER;
static struct Queue *sg_writeQueue;
static struct Queue *sg_readQueue;

static void CommandAndControl_Thread (struct Thread *p_pThread);
static int CommandAndControl_DispatchCommand (struct RazorbackContext *, void *);
static bool CommandAndControl_Register (struct RazorbackContext *);
static int CommandAndControl_SendHello (struct RazorbackContext *p_pContext, void *);
static void CommandAndControl_HelloTimer (union sigval val);
static bool CommandAndControl_ArmHelloTimer (void);
static bool CommandAndControl_processHelloMessage (struct Message *);
static bool CommandAndControl_processCacheClearMessage (struct RazorbackContext *context);


static bool Default_processRegReqMessage (struct Message *);
static bool Default_processRegRespMessage (struct Message *);
static bool Default_processRegErrMessage (struct Message *);
static bool Default_processConfUpdateMessage (struct Message *);
static bool Default_processConfAckMessage (struct Message *);
static bool Default_processConfErrMessage (struct Message *);
static bool Default_processPauseMessage (struct Message *);
static bool Default_processPausedMessage (struct Message *);
static bool Default_processGoMessage (struct Message *);
static bool Default_processRunningMessage (struct Message *);
static bool Default_processTermMessage (struct Message *);
static bool Default_processByeMessage (struct Message *);

static struct RazorbackCommandAndControlHooks sg_DefaultHooks = {
    Default_processRegReqMessage,
    Default_processRegRespMessage,
    Default_processRegErrMessage,
    Default_processConfUpdateMessage,
    Default_processConfAckMessage,
    Default_processConfErrMessage,
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
            if ((sg_readQueue = Queue_Create (COMMAND_QUEUE, QUEUE_FLAG_RECV)) == NULL)
            {
                rzb_log (LOG_ERR, "%s: C&C Error: Failed to connect to MQ.", __func__);
                return false;
            }
            if ((sg_writeQueue = Queue_Create (COMMAND_QUEUE, QUEUE_FLAG_SEND)) == NULL)
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

void 
CommandAndControl_Shutdown(void)
{
   // Shut down state tracking timer.
   ConnectedEntityList_Stop();

   // Disable the hello sending timer.
   timer_delete(sg_tHelloTimer);

   // Shutdown the C&C Thread.
   Thread_InterruptAndJoin(sg_tThread);
}

static void 
CommandAndControl_Thread (struct Thread *p_pThread)
{
    // local variables for command processing
    struct RazorbackContext *context, *prevContext;
    struct Message *message;
    uuid_t dest;
    uuid_t source;
    while (!Thread_IsStopped(p_pThread))
    {
        if ((message = Queue_Get (sg_readQueue)) == NULL)
        {
            // timeout
            if (errno == EAGAIN || errno == EINTR)
                continue;
            // error
            rzb_log (LOG_ERR,
                     "%s: Dropped command due to failure of CommandQueue_Get()", __func__);
            // drop message
            continue;
        }
        pthread_mutex_lock(&processLock);
        if (Message_CnC_Get_Nuggets(message, source, dest) == false)
        {
            rzb_log(LOG_ERR, "%s: Dropped command, failed to parse source/dest UUID", __func__);
            message->destroy(message);
            continue;
        }

        if (uuid_is_null (dest) == 1)    // Broadcast Message
        {
            if (Razorback_LookupContext (source) == NULL)
                Razorback_ForEach_Context (CommandAndControl_DispatchCommand, message);
        }
        else                    // Directed Message
        {
            if ((context = Razorback_LookupContext (dest)) != NULL)
            {
                prevContext = Thread_GetContext(p_pThread);
                Thread_ChangeContext(p_pThread, context);
                CommandAndControl_DispatchCommand (context, message);
                Thread_ChangeContext(p_pThread, prevContext);
            }
        }
        pthread_mutex_unlock(&processLock);
        message->destroy(message);
    }
    Queue_Terminate(sg_readQueue);
    Queue_Terminate(sg_writeQueue);
    rzb_log(LOG_DEBUG, "C&C Thread Exiting");
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

static int
CommandAndControl_DispatchCommand (struct RazorbackContext *p_pContext, void*userData)
{
    struct Message *message;
    ASSERT (userData != NULL);
    ASSERT (p_pContext->pCommandHooks != NULL);

    if (userData == NULL)
        return LIST_EACH_ERROR;
    if (p_pContext->pCommandHooks == NULL)
        return LIST_EACH_ERROR;
    
    message = userData;
#ifdef CNC_DEBUG
    uuid_t source, dest;
    char l_sUuidSource[UUID_STRING_LENGTH], l_sUuidDest[UUID_STRING_LENGTH];
    uint32_t l_iDataTypeIttr;
#endif //CNC_DEBUG

    if (p_pContext->pCommandHooks == NULL)
    {
        rzb_log (LOG_ERR, "%s: Command Dropped: C&C Hooks NULL", __func__);
        return LIST_EACH_ERROR;
    }
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG,
             "%s: C&C Message Dispatch: Type: 0x%08x Length: 0x%08x Version: 0x%08x", __func__, 
             message->type,
             message->length,
             message->version);

    if (rzb_get_log_level () == LOG_DEBUG)
    {
        Message_CnC_Get_Nuggets(message, source, dest);
        uuid_unparse (source, l_sUuidSource);
        uuid_unparse (dest, l_sUuidDest);
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Source %s Dest: %s", __func__,
                 l_sUuidSource, l_sUuidDest);
    }
#endif //CNC_DEBUG

    switch (message->type)
    {
    case MESSAGE_TYPE_HELLO:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: ==== Start Hello Message =====", __func__);
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Nugget Type UUID: %s Desc: %s",
                ((struct MessageHello *)message->message)->uuidNuggetType, UUID_TYPE_NUGGET_TYPE);
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Application Type UUID: %s Desc: %s",
                ((struct MessageHello *)message->message)->uuidApplicationType, UUID_TYPE_NUGGET);
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: ==== End Hello Message ====", __func__);
#endif //CNC_DEBUG
        if (CommandAndControl_processHelloMessage (message))
            return LIST_EACH_OK;
        else
            return LIST_EACH_ERROR;
                                                      
        break;
    case MESSAGE_TYPE_REG_REQ:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG,
                 "%s: C&C Msg Dispatch: ==== Start Registration Request Message ====", __func__);
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Nugget Type UUID: %s Desc: %s",
                ((struct MessageRegistrationRequest *)message->message)->uuidNuggetType, UUID_TYPE_NUGGET_TYPE);
        CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Application Type UUID: %s Desc: %s",
                ((struct MessageRegistrationRequest *)message->message)->uuidApplicationType, UUID_TYPE_NUGGET_TYPE);
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Data Type Count: %i", __func__,
                ((struct MessageRegistrationRequest *)message->message)->iDataTypeCount);
        for (l_iDataTypeIttr = 0;
             l_iDataTypeIttr < ((struct MessageRegistrationRequest *)message->message)->iDataTypeCount;
             l_iDataTypeIttr++)
        {
            CommandAndControl_PrintUuid (LOG_DEBUG, "C&C Msg Dispatch: Data Type UUID: %s Desc: %s",
                                            ((struct MessageRegistrationRequest *)message->message)->
                                            pDataTypeList[l_iDataTypeIttr],
                                            UUID_TYPE_DATA_TYPE);
        }
        rzb_log (LOG_DEBUG,
                 "%s: C&C Msg Dispatch: ==== End Registration Request Message ====", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRegReqMessage != NULL)
        {
            if (p_pContext->
                pCommandHooks->processRegReqMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
                return LIST_EACH_ERROR;
        }
        break;
    case MESSAGE_TYPE_REG_RESP:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG,
                 "%s: C&C Msg Dispatch: Registration Response Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRegRespMessage != NULL)
        {
            if ( p_pContext->
                pCommandHooks->processRegRespMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
        break;
    case MESSAGE_TYPE_REG_ERR:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Registration Error Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRegErrMessage != NULL)
        {
            if ( p_pContext->
                pCommandHooks->processRegErrMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
        break;
    case MESSAGE_TYPE_CONFIG_UPDATE:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Configuration Update Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processConfUpdateMessage != NULL)
        {
            if ( p_pContext->
                pCommandHooks->processConfUpdateMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
        break;
    case MESSAGE_TYPE_CONFIG_ACK:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Configuration Ack Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processConfAckMessage != NULL)
        {
            if ( p_pContext->
                pCommandHooks->processConfAckMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
        break;
    case MESSAGE_TYPE_CONFIG_ERR:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Configuration Error Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processConfErrMessage != NULL)
        {
            if ( p_pContext->
                pCommandHooks->processConfErrMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
        break;
    case MESSAGE_TYPE_PAUSE:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Pause Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processPauseMessage != NULL)
        {
            if ( p_pContext->
                pCommandHooks->processPauseMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
        break;
    case MESSAGE_TYPE_PAUSED:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Paused Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processPausedMessage != NULL)
        {
            if ( p_pContext->
                pCommandHooks->processPausedMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
        break;
    case MESSAGE_TYPE_GO:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Go Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processGoMessage != NULL)
        {
            if ( p_pContext->
                pCommandHooks->processGoMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
        break;
    case MESSAGE_TYPE_RUNNING:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Running Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processRunningMessage != NULL)
        {
            if ( p_pContext->
                pCommandHooks->processRunningMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
        break;
    case MESSAGE_TYPE_TERM:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Terminate Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processTermMessage != NULL)
        {
            if (p_pContext->
                pCommandHooks->processTermMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
        break;
    case MESSAGE_TYPE_BYE:
#ifdef CNC_DEBUG
        rzb_log (LOG_DEBUG, "%s: C&C Msg Dispatch: Bye Message", __func__);
#endif //CNC_DEBUG
        if (p_pContext->pCommandHooks->processByeMessage != NULL)
        {
            if(p_pContext->
                pCommandHooks->processByeMessage (message))
            {
                return LIST_EACH_OK;
            }
            else
            {
                return LIST_EACH_ERROR;
            }
        }
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
                 message->type);
        return LIST_EACH_ERROR;
    }
    return LIST_EACH_OK;
}

bool 
CommandAndControl_SendBye (struct RazorbackContext *context)
{
    struct Message *bye;
    if (( bye = MessageBye_Initialize(
            context->uuidNuggetId)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to create bye message", __func__);
        return false;
    }

    if (!Queue_Put (sg_writeQueue, bye))
    {
        bye->destroy(bye);
        rzb_log (LOG_ERR, "%s: Failed to send bye message", __func__);
        return false;
    }
    bye->destroy(bye);
    return true;
}

static bool
CommandAndControl_Register (struct RazorbackContext *p_pContext)
{
    struct Message *regReq;

    if ((regReq = MessageRegistrationRequest_Initialize (
                                                p_pContext->uuidNuggetId,
                                                p_pContext->uuidNuggetType,
                                                p_pContext->uuidApplicationType,
                                                p_pContext->iDataTypeCount,
                                                p_pContext->pDataTypeList)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: C&C Register: Failed to Init Registration Request", __func__);
        return false;
    }

    if (!Queue_Put (sg_writeQueue, regReq))
    {
        rzb_log (LOG_ERR,
                 "%s: C&C Register: Failed to send registration Request", __func__);
        regReq->destroy(regReq);
        return false;
    }
    regReq->destroy(regReq);
    // Wait for Go Message
    sem_wait(&p_pContext->regSem);
    return p_pContext->regOk;
}

static int
CommandAndControl_SendHello (struct RazorbackContext *p_pContext, void *userData)
{
    struct Message *hello;
    if ((hello = MessageHello_Initialize (
                             p_pContext->uuidNuggetId,
                             p_pContext->uuidNuggetType,
                             p_pContext->uuidApplicationType)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to create message", __func__);
        return LIST_EACH_ERROR;
    }

    if (!Queue_Put (sg_writeQueue, hello))
    {
        rzb_log (LOG_ERR, "%s: C&C Hello Timer: Failed to send Hello Message", __func__);
        hello->destroy(hello);
        return LIST_EACH_ERROR;
    }
    hello->destroy(hello);
    return LIST_EACH_OK;
}

static void
CommandAndControl_HelloTimer (union sigval val)
{
    CommandAndControl_Pause();
    Razorback_ForEach_Context (CommandAndControl_SendHello, NULL);
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
CommandAndControl_processHelloMessage (struct Message *message)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Hook: Hello", __func__);
#endif //CNC_DEBUG
    struct MessageHello *hello = message->message;
    struct RazorbackContext *l_pContext = NULL;
    uint32_t l_iState =0;
    struct Message *term;
    char *l_sNugType, *l_sAppType, l_sNugId[UUID_STRING_LENGTH];
    uuid_t source,dest;


    l_pContext = Thread_GetContext(sg_tThread);

    if ((l_pContext->iFlags & CONTEXT_FLAG_DISPATCHER) ==
        CONTEXT_FLAG_DISPATCHER)
    {
        Message_CnC_Get_Nuggets(message,source,dest);
        ConnectedEntityList_GetState (source, &l_iState);
        if (l_iState == 0) 
        {
            l_sNugType = UUID_Get_NameByUUID(hello->uuidNuggetType, UUID_TYPE_NUGGET_TYPE);
            l_sAppType = UUID_Get_NameByUUID(hello->uuidApplicationType, UUID_TYPE_NUGGET);
            uuid_unparse(source, l_sNugId);
            rzb_log(LOG_INFO, "%s: Hello recieved from unregistered nugget (termination requested): %s. %s, %s", __func__, l_sNugType, l_sAppType, l_sNugId);
            free(l_sNugType);
            free(l_sAppType);
            // TODO: This should be a re-register message not terminate.
            if ((term = MessageTerminate_Initialize(dest,source,
                (uint8_t *)"Not Registered")) == NULL)
            {
                rzb_log (LOG_ERR, "%s: C&C Hello Processor: Failed to create Terminate Message", __func__);
                return false;
            }
            if (!Queue_Put (sg_writeQueue, term))
            {
                term->destroy(term);
                rzb_log (LOG_ERR, "%s: C&C Hello Processor: Failed to send Terminate Message", __func__);
                return false;
            }
            term->destroy(term);
            return true;
        }
        // TODO: Track number of dispatchers online
        ConnectedEntityList_Update (source);
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
Default_processRegReqMessage (struct Message *mrrRegReq)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Reg Req", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processRegRespMessage (struct Message *message)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Reg Resp", __func__);
#endif //CNC_DEBUG
    struct RazorbackContext *l_pContext = Thread_GetContext(sg_tThread);

    if ((l_pContext->iFlags & CONTEXT_FLAG_STAND_ALONE) ==
        CONTEXT_FLAG_STAND_ALONE)
    {
        CommandAndControl_SendHello(l_pContext, NULL);
        CommandAndControl_ArmHelloTimer ();
    }

    return true;
}

static bool
Default_processRegErrMessage (struct Message *message)
{
    struct RazorbackContext *context;
    struct MessageError *error = message->message;
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Reg Err", __func__);
#endif //CNC_DEBUG
    rzb_log(LOG_ERR, "%s: Registration Error: %s", __func__, error->sMessage);
    context = Thread_GetContext(sg_tThread);
    // Wait up the registering thread.
    context->regOk = false;
    sem_post(&context->regSem);
    return true;
}

static void
CnC_UpdateUUIDList(int listType, uint32_t count, char* names, uuid_t* uuids)
{
    struct List *list;
    char *curStr;
    uuid_t *curUuid;
    uint32_t pos = 0;

    list = UUID_Get_List(listType);
    List_Lock(list);
    List_Clear(list);
    curStr = names;
    curUuid = uuids;
    for (pos =0; pos < count; pos++)
    {
        UUID_Add_List_Entry(list, *curUuid, curStr, "");
        curStr = curStr + strlen(curStr) +1;
        curUuid+=1;
    }
    List_Unlock(list);
}
static bool
Default_processConfUpdateMessage (struct Message *message)
{
    struct MessageConfigurationUpdate *mcuConfigUpdate = message->message;
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Conf Update", __func__);
#endif //CNC_DEBUG
    struct RazorbackContext *l_pContext;
    struct Message *configAck;
    uuid_t source,dest;

    CnC_UpdateUUIDList(UUID_TYPE_NTLV_TYPE,
            mcuConfigUpdate->ntlvTypesCount,
            mcuConfigUpdate->ntlvTypesNames,
            mcuConfigUpdate->ntlvTypesUuids);

    CnC_UpdateUUIDList(UUID_TYPE_NTLV_NAME,
            mcuConfigUpdate->ntlvNamesCount,
            mcuConfigUpdate->ntlvNamesNames,
            mcuConfigUpdate->ntlvNamesUuids);

    CnC_UpdateUUIDList(UUID_TYPE_DATA_TYPE,
            mcuConfigUpdate->dataTypesCount,
            mcuConfigUpdate->dataTypesNames,
            mcuConfigUpdate->dataTypesUuids);

    if ((l_pContext = Thread_GetContext(sg_tThread)) == NULL)
    {
        rzb_log (LOG_ERR,
                 "%s: C&C Config Update: Failed to get valid context for message", __func__);
    }
    Message_CnC_Get_Nuggets(message, source,dest);
    if ((configAck = MessageConfigurationAck_Initialize(
                             l_pContext->uuidNuggetId,
                             source,
                             l_pContext->uuidNuggetType,
                             l_pContext->uuidApplicationType)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to create config ack message", __func__);
        return false;
    }

    if (!Queue_Put (sg_writeQueue, configAck))
    {
        rzb_log (LOG_ERR, "%s: C&C Config Update: Failed to send Configuration Ack Message", __func__);
        configAck->destroy(configAck);
        return false;
    }
    configAck->destroy(configAck);
    return true;
}

static bool
Default_processConfAckMessage (struct Message *message)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Conf Ack", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processConfErrMessage (struct Message *message)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Conf Err", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processPauseMessage (struct Message *message)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Pause", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processPausedMessage (struct Message *message)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Paused", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processGoMessage (struct Message *message)
{
    struct RazorbackContext *l_pContext;
    struct Message *running;
    uuid_t source,dest;

#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Go", __func__);
#endif //CNC_DEBUG

    if ((l_pContext = Thread_GetContext(sg_tThread)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Recieved go message for unknown context", __func__);
        return false;
    }
    
    // Wait up the registering thread.
    l_pContext->regOk = true;
    sem_post(&l_pContext->regSem);

    Message_CnC_Get_Nuggets(message, source,dest);

    if ((running = MessageRunning_Initialize(dest, source)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Failed to create running message", __func__);
        return false;
    }
    if (!Queue_Put (sg_writeQueue, running))
    {
        rzb_log (LOG_ERR, "%s: Go Hook: Failed to send running message", __func__);
        running->destroy(running);
        return false;
    }
    running->destroy(running);
    return true;
}

static bool
Default_processRunningMessage (struct Message *message)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Running", __func__);
#endif //CNC_DEBUG
    return true;
}

static bool
Default_processTermMessage (struct Message *message)
{
    struct MessageTerminate *term = message->message;
    struct Message *bye;
    struct RazorbackContext *context;
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Terminate", __func__);
#endif //CNC_DEBUG
    rzb_log(LOG_INFO, "%s: Termination Requested: %s", __func__, term->sTerminateReason);
    context = Thread_GetContext(sg_tThread);

    if ((bye = MessageBye_Initialize(context->uuidNuggetId)) == NULL)
    {
        rzb_log(LOG_ERR, "%s: Terminate Hook: Failed to alloc bye message", __func__);
    }
    else
    {
        if (!Queue_Put (sg_writeQueue, bye))
        {
            rzb_log (LOG_ERR, "%s: Terminate Hook: Failed to send bye message", __func__);
        }
        bye->destroy(bye);
    }
    exit(0); // TODO: This should be a clean shutdown
    return true;
}

static bool
Default_processByeMessage (struct Message *message)
{
#ifdef CNC_DEBUG
    rzb_log (LOG_DEBUG, "%s: C&C Default Hook: Bye", __func__);
#endif //CNC_DEBUG
    return true;
}
