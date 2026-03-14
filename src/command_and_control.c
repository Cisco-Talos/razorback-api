/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

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
#include <razorback/timer.h>
#include <razorback/thread_pool.h>
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
static Thread_t *sg_tThread;
static struct Timer * sg_helloTimer;

Mutex_t * sg_mPauseLock = NULL;
Mutex_t * processLock = NULL;

static struct Queue *sg_writeQueue;
static struct Queue *sg_readQueue;

static void CommandAndControl_Thread (Thread_t *p_pThread);
static int CommandAndControl_DispatchCommand (struct RazorbackContext *, void *);
static bool CommandAndControl_Register (struct RazorbackContext *);
static int CommandAndControl_SendHello (struct RazorbackContext *p_pContext, void *);
static void CommandAndControl_HelloTimer (void * val);
SO_PUBLIC bool CommandAndControl_ArmHelloTimer (void);
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
static bool Default_processHelloMessage (struct Message *);
static bool Default_processReRegMessage (struct Message *);

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
    Default_processByeMessage,
    Default_processHelloMessage,
    Default_processReRegMessage
};


void
CommandAndControl_Pause(void) {
    Mutex_Lock(processLock);
}

void
CommandAndControl_Unpause(void) {
    Mutex_Unlock(processLock);
}

bool
CommandAndControl_Start (struct RazorbackContext *p_pContext) {
    uuid_t dispatcher;
    ASSERT (p_pContext != NULL);
    if (p_pContext == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Context is NULL", __func__);
        return false;
    }

    if (!UUID_Get_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE, dispatcher)) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to get dispatcher UUID", __func__);
        return false;
    }

    if (p_pContext->pCommandHooks == NULL)
        p_pContext->pCommandHooks = &sg_DefaultHooks;


    if ((p_pContext->iFlags & CONTEXT_FLAG_STAND_ALONE) ==
        CONTEXT_FLAG_STAND_ALONE) {
        if (!sg_bQueueInitialized) {
            if ((sg_mPauseLock = Mutex_Create(MUTEX_MODE_NORMAL)) == NULL)
                return false;
            if ((processLock = Mutex_Create(MUTEX_MODE_NORMAL)) == NULL)
                return false;

            if ((sg_readQueue = Queue_Create(COMMAND_QUEUE, true, QUEUE_FLAG_RECV)) == NULL) {
                rzb_log(LOG_ERR, LOG_C_CNC, "%s: C&C Error: Failed to connect to MQ.", __func__);
                return false;
            }
            if ((sg_writeQueue = Queue_Create(COMMAND_QUEUE, true, QUEUE_FLAG_SEND)) == NULL) {
                rzb_log(LOG_ERR, LOG_C_CNC, "%s: C&C Error: Failed to connect to MQ.", __func__);
                return false;
            }

            ConnectedEntityList_Start();

            sg_bQueueInitialized = true;
            if ((sg_tThread =
                         Thread_Launch(CommandAndControl_Thread, NULL,
                                       "Command and Control", p_pContext)) == NULL) {
                rzb_log(LOG_ERR, LOG_C_CNC, "%s: C&C Error: Failed to launch C&C thread.", __func__);
                return false;
            }

        }
    } else {
        if (!sg_bQueueInitialized) {
            rzb_log(LOG_ERR, LOG_C_CNC,
                    "%s: C&C Error: Can't start child context without a "
                    "running master context", __func__);
            return false;
        }

    }
    // Kick Start Registration
    if (uuid_compare(dispatcher, p_pContext->uuidNuggetType) != 0) {
        return CommandAndControl_Register(p_pContext);
    }

    return true;
}

void
CommandAndControl_Shutdown(void) {
   // Shut down state tracking timer.
   ConnectedEntityList_Stop();

   // Disable the hello sending timer.
   Timer_Destroy(sg_helloTimer);

   // Shutdown the C&C Thread.
   Thread_InterruptAndJoin(sg_tThread);
   Thread_Destroy(sg_tThread);
   sg_tThread = NULL;
}

static void
CommandAndControl_Thread (Thread_t *p_pThread) {

    // local variables for command processing
    struct RazorbackContext *context, *prevContext;
    struct Message *message;
    uuid_t dest;
    uuid_t source;

    ASSERT (p_pThread != NULL);
    if (p_pThread == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Thread is NULL", __func__);
        return;
    }

    while (!Thread_IsStopped(p_pThread)) {

        if ((message = Queue_Get(sg_readQueue)) == NULL) {
            // timeout
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            // error
            rzb_perror(LOG_C_CNC,
                    "Dropped command due to failure of CommandQueue_Get(%s)");
            // drop message
            continue;
        }

        // Lock out other threads from the stomp connections
        CommandAndControl_Pause();
        if (Message_Get_Nuggets(message, source, dest) == false) {
            rzb_log(LOG_ERR, LOG_C_CNC, "%s: Dropped command, failed to parse source/dest UUID", __func__);
            message->destroy(message);
            continue;
        }

        if (uuid_is_null(dest) == 1) {    // Broadcast Message
            if (Razorback_LookupContext(source) == NULL) {
                Razorback_ForEach_Context(CommandAndControl_DispatchCommand, message);
            }
        } else { // Directed Message
            if ((context = Razorback_LookupContext(dest)) != NULL) {
                prevContext = Thread_GetContext(p_pThread);
                Thread_ChangeContext(p_pThread, context);
                CommandAndControl_DispatchCommand(context, message);
                Thread_ChangeContext(p_pThread, prevContext);
            }
        }
        // Let other threads use the stomp connection
        CommandAndControl_Unpause();
        message->destroy(message);
    }
    Queue_Terminate(sg_readQueue);
    Queue_Terminate(sg_writeQueue);
    rzb_log(LOG_DEBUG, LOG_C_CNC, "C&C Thread Exiting");
    return;
}

static void
CommandAndControl_PrintUuid(int p_iLevel, const char *p_fmt, uuid_t p_uuid, uint32_t p_iUuidType) {
    char *l_sUuidDesc;
    char l_sUuid[UUID_STRING_LENGTH];
    uuid_unparse(p_uuid, l_sUuid);
    l_sUuidDesc =
            UUID_Get_DescriptionByUUID(p_uuid, p_iUuidType);
    if (l_sUuidDesc == NULL) {
        rzb_log(p_iLevel, LOG_C_CNC, p_fmt, l_sUuid, "(NULL)");
    } else {
        rzb_log(p_iLevel, LOG_C_CNC, p_fmt, l_sUuid, l_sUuidDesc);
        free(l_sUuidDesc);
    }
}

static int
CommandAndControl_DispatchCommand (struct RazorbackContext *p_pContext, void*userData) {
    struct Message *message;
    uuid_t source, dest;
    char l_sUuidSource[UUID_STRING_LENGTH], l_sUuidDest[UUID_STRING_LENGTH];
    uint32_t l_iDataTypeIttr;

    ASSERT(userData != NULL);
    ASSERT(p_pContext->pCommandHooks != NULL);

    if (userData == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Command Dropped: Message NULL", __func__);
        return LIST_EACH_ERROR;
    }
    if (p_pContext->pCommandHooks == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Command Dropped: C&C Hooks NULL", __func__);
        return LIST_EACH_ERROR;
    }

    message = userData;


    if (p_pContext->pCommandHooks == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Command Dropped: C&C Hooks NULL", __func__);
        return LIST_EACH_ERROR;
    }

    rzb_log(LOG_DEBUG, LOG_C_CNC,
            "%s: C&C Message Dispatch: Type: 0x%08x Length: 0x%08x Version: 0x%08x", __func__,
            message->type,
            message->length,
            message->version);

    if (rzb_get_log_level() == LOG_DEBUG) {
        Message_Get_Nuggets(message, source, dest);
        uuid_unparse(source, l_sUuidSource);
        uuid_unparse(dest, l_sUuidDest);
        rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Source %s Dest: %s", __func__,
                l_sUuidSource, l_sUuidDest);
    }

    switch (message->type) {
        case MESSAGE_TYPE_HELLO:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: ==== Start Hello Message =====", __func__);
            CommandAndControl_PrintUuid(LOG_DEBUG, "C&C Msg Dispatch: Nugget Type UUID: %s Desc: %s",
                                        ((struct MessageHello *) message->message)->uuidNuggetType,
                                        UUID_TYPE_NUGGET_TYPE);
            CommandAndControl_PrintUuid(LOG_DEBUG, "C&C Msg Dispatch: Application Type UUID: %s Desc: %s",
                                        ((struct MessageHello *) message->message)->uuidApplicationType,
                                        UUID_TYPE_NUGGET);
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: ==== End Hello Message ====", __func__);
            if (p_pContext->pCommandHooks->processHelloMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processHelloMessage(message)) {
                    return LIST_EACH_OK;
                } else
                    return LIST_EACH_ERROR;
            }
            break;
        case MESSAGE_TYPE_REG_REQ:
            rzb_log(LOG_DEBUG, LOG_C_CNC,
                    "%s: C&C Msg Dispatch: ==== Start Registration Request Message ====", __func__);
            CommandAndControl_PrintUuid(LOG_DEBUG, "C&C Msg Dispatch: Nugget Type UUID: %s Desc: %s",
                                        ((struct MessageRegistrationRequest *) message->message)->uuidNuggetType,
                                        UUID_TYPE_NUGGET_TYPE);
            CommandAndControl_PrintUuid(LOG_DEBUG, "C&C Msg Dispatch: Application Type UUID: %s Desc: %s",
                                        ((struct MessageRegistrationRequest *) message->message)->uuidApplicationType,
                                        UUID_TYPE_NUGGET_TYPE);
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Data Type Count: %i", __func__,
                    ((struct MessageRegistrationRequest *) message->message)->iDataTypeCount);
            for (l_iDataTypeIttr = 0;
                 l_iDataTypeIttr < ((struct MessageRegistrationRequest *) message->message)->iDataTypeCount;
                 l_iDataTypeIttr++) {
                CommandAndControl_PrintUuid(LOG_DEBUG, "C&C Msg Dispatch: Data Type UUID: %s Desc: %s",
                                            ((struct MessageRegistrationRequest *) message->message)->
                                                    pDataTypeList[l_iDataTypeIttr],
                                            UUID_TYPE_DATA_TYPE);
            }
            rzb_log(LOG_DEBUG, LOG_C_CNC,
                    "%s: C&C Msg Dispatch: ==== End Registration Request Message ====", __func__);
            if (p_pContext->pCommandHooks->processRegReqMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processRegReqMessage(message)) {
                    return LIST_EACH_OK;
                } else
                    return LIST_EACH_ERROR;
            }
            break;
        case MESSAGE_TYPE_REG_RESP:
            rzb_log(LOG_DEBUG, LOG_C_CNC,
                    "%s: C&C Msg Dispatch: Registration Response Message", __func__);
            if (p_pContext->pCommandHooks->processRegRespMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processRegRespMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_REG_ERR:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Registration Error Message", __func__);
            if (p_pContext->pCommandHooks->processRegErrMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processRegErrMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_CONFIG_UPDATE:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Configuration Update Message", __func__);
            if (p_pContext->pCommandHooks->processConfUpdateMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processConfUpdateMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_CONFIG_ACK:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Configuration Ack Message", __func__);
            if (p_pContext->pCommandHooks->processConfAckMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processConfAckMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_CONFIG_ERR:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Configuration Error Message", __func__);
            if (p_pContext->pCommandHooks->processConfErrMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processConfErrMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_PAUSE:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Pause Message", __func__);
            if (p_pContext->pCommandHooks->processPauseMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processPauseMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_PAUSED:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Paused Message", __func__);
            if (p_pContext->pCommandHooks->processPausedMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processPausedMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_GO:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Go Message", __func__);
            if (p_pContext->pCommandHooks->processGoMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processGoMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_RUNNING:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Running Message", __func__);
            if (p_pContext->pCommandHooks->processRunningMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processRunningMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_TERM:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Terminate Message", __func__);
            if (p_pContext->pCommandHooks->processTermMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processTermMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_BYE:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Bye Message", __func__);
            if (p_pContext->pCommandHooks->processByeMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processByeMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        case MESSAGE_TYPE_CLEAR:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Cache Clear Message", __func__);
            return CommandAndControl_processCacheClearMessage(p_pContext);
            break;
        case MESSAGE_TYPE_REREG:
            rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Msg Dispatch: Re-Register Message", __func__);
            if (p_pContext->pCommandHooks->processReRegMessage != NULL) {
                if (p_pContext->
                        pCommandHooks->processReRegMessage(message)) {
                    return LIST_EACH_OK;
                } else {
                    return LIST_EACH_ERROR;
                }
            }
            break;
        default:
            rzb_log(LOG_ERR, LOG_C_CNC,
                    "%s: Dropped C&C Message: Bad Type (%i)", __func__,
                    message->type);
            return LIST_EACH_ERROR;
    }
    return LIST_EACH_OK;
}

bool
CommandAndControl_SendBye (struct RazorbackContext *context) {
    struct Message *bye;

    ASSERT (context != NULL);
    if (context == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Context is NULL", __func__);
        return false;
    }

    if ((bye = MessageBye_Initialize(
            context->uuidNuggetId)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to create bye message", __func__);
        return false;
    }

    if (!Queue_Put(sg_writeQueue, bye)) {
        bye->destroy(bye);
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to send bye message", __func__);
        return false;
    }
    bye->destroy(bye);
    return true;
}

static bool
CommandAndControl_Register (struct RazorbackContext *p_pContext) {
    struct Message *regReq;
    struct ConnectedEntity *dispatcher = NULL;

    ASSERT (p_pContext != NULL);
    if (p_pContext == NULL) {
        rzb_log (LOG_ERR, LOG_C_CNC, "%s: Context is NULL", __func__);
        return false;
    }
    while ((dispatcher = ConnectedEntityList_GetDedicatedDispatcher()) == NULL) {
        rzb_log(LOG_INFO, LOG_C_CNC, "%s: Waiting for dispatcher", __func__);
#ifdef _MSC_VER
        Sleep(1000);
#else
        sleep(1);
#endif
    }

    if ((regReq = MessageRegistrationRequest_Initialize(
            dispatcher->uuidNuggetId,
            p_pContext->uuidNuggetId,
            p_pContext->uuidNuggetType,
            p_pContext->uuidApplicationType,
            p_pContext->inspector.dataTypeCount,
            p_pContext->inspector.dataTypeList)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC,
                "%s: C&C Register: Failed to Init Registration Request", __func__);
        return false;
    }
    ConnectedEntity_Destroy(dispatcher);
    if (!Queue_Put(sg_writeQueue, regReq)) {
        rzb_log(LOG_ERR, LOG_C_CNC,
                "%s: C&C Register: Failed to send registration Request", __func__);
        regReq->destroy(regReq);
        return false;
    }
    regReq->destroy(regReq);
    // Wait for Go Message
    Semaphore_Wait(p_pContext->regSem);
    return p_pContext->regOk;
}

static int
CommandAndControl_SendHello (struct RazorbackContext *p_pContext, void *userData) {
    struct Message *hello;
    ASSERT (p_pContext != NULL);
    if (p_pContext == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Context is NULL", __func__);
        return LIST_EACH_ERROR;
    }

    if ((hello = MessageHello_Initialize (p_pContext)) == NULL)
    {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to create message", __func__);
        return LIST_EACH_ERROR;
    }

    if (!Queue_Put (sg_writeQueue, hello))
    {
        rzb_log (LOG_ERR, LOG_C_CNC, "%s: C&C Hello Timer: Failed to send Hello Message", __func__);
        hello->destroy(hello);
        return LIST_EACH_ERROR;
    }
    hello->destroy(hello);
    return LIST_EACH_OK;
}

static void
CommandAndControl_HelloTimer (void *val) {
    CommandAndControl_Pause();
    Razorback_ForEach_Context (CommandAndControl_SendHello, NULL);
    CommandAndControl_Unpause();
}

SO_PUBLIC bool
CommandAndControl_ArmHelloTimer (void) {
    sg_helloTimer = Timer_Create(Config_getHelloTime(), CommandAndControl_HelloTimer, NULL);
    if (sg_helloTimer == NULL) {
        return false;
    }
    return true;
}

static bool
Default_processHelloMessage (struct Message *message) {
    struct MessageHello *hello;
    uuid_t source,dest;
    uuid_t dispatcher;
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }

    hello = message->message;
    //struct RazorbackContext *l_pContext = NULL;


    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Hook: Hello", __func__);
    UUID_Get_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE, dispatcher);


    //l_pContext = Thread_GetContext(sg_tThread);
    Message_Get_Nuggets(message,source,dest);

    // Only care about dispatchers
    if (uuid_compare(dispatcher, hello->uuidNuggetType) != 0) {
        return true;
    }

    ConnectedEntityList_Update (message);

    return true;
}

static bool
CommandAndControl_processCacheClearMessage (struct RazorbackContext *context) {
    uuid_t dispatcher;
    ASSERT (context != NULL);
    if (context == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Context is NULL", __func__);
        return false;
    }
    if (!UUID_Get_UUID(NUGGET_TYPE_DISPATCHER, UUID_TYPE_NUGGET_TYPE, dispatcher)) {
        rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: Failed to get dispatcher UUID", __func__);
        return false;
    }

    // If we are not a dispatcher but we are stand alone then process this request.
    if (
            (uuid_compare(dispatcher, context->uuidNuggetType) != 0) &&
            ((context->iFlags & CONTEXT_FLAG_STAND_ALONE) == CONTEXT_FLAG_STAND_ALONE)
            ) {
        rzb_log(LOG_INFO, LOG_C_CNC, "%s: Clearing Local Cache", __func__);
        clearLocalEntry(ALL, FULL);
    }
    return true;
}


static bool
Default_processRegReqMessage (struct Message *mrrRegReq) {
    ASSERT(mrrRegReq != NULL);
    if (mrrRegReq == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }
    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Reg Req", __func__);
    return true;
}

static bool
Default_processRegRespMessage (struct Message *message)
{
    struct RazorbackContext *l_pContext = Thread_GetContext(sg_tThread);

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }

    rzb_log(LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Reg Resp", __func__);
    if ((l_pContext->iFlags & CONTEXT_FLAG_STAND_ALONE) ==
        CONTEXT_FLAG_STAND_ALONE) {
        CommandAndControl_SendHello(l_pContext, NULL);
        CommandAndControl_ArmHelloTimer();
    }

    return true;
}

static bool
Default_processRegErrMessage (struct Message *message) {
    struct RazorbackContext *context;
    struct MessageError *error;
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }
    error = message->message;
    rzb_log(LOG_ERR, LOG_C_CNC, "%s: Registration Error: %s", __func__, error->sMessage);
    context = Thread_GetContext(sg_tThread);
    // Wait up the registering thread.
    context->regOk = false;
    Semaphore_Post(context->regSem);
    return true;
}

static void
CnC_UpdateUUIDList(int listType, List_t *msgList)
{
    List_t *list;
    ASSERT(msgList != NULL);
    if (msgList == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message Source List is NULL", __func__);
        return;
    }
    list = UUID_Get_List(listType);
    List_Transfer(list, msgList);
}

static bool
Default_processConfUpdateMessage (struct Message *message){
    struct MessageConfigurationUpdate *mcuConfigUpdate;
    struct RazorbackContext *l_pContext;
    struct Message *configAck;
    uuid_t source,dest;

    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }

    mcuConfigUpdate = message->message;
    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Conf Update", __func__);


    CnC_UpdateUUIDList(UUID_TYPE_NTLV_TYPE, mcuConfigUpdate->ntlvTypes);

    CnC_UpdateUUIDList(UUID_TYPE_NTLV_NAME, mcuConfigUpdate->ntlvNames);

    CnC_UpdateUUIDList(UUID_TYPE_DATA_TYPE, mcuConfigUpdate->dataTypes);

    if ((l_pContext = Thread_GetContext(sg_tThread)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC,
                "%s: C&C Config Update: Failed to get valid context for message", __func__);
    }
    Message_Get_Nuggets(message, source, dest);
    if ((configAck = MessageConfigurationAck_Initialize(
            l_pContext->uuidNuggetId,
            source,
            l_pContext->uuidNuggetType,
            l_pContext->uuidApplicationType)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to create config ack message", __func__);
        return false;
    }

    if (!Queue_Put(sg_writeQueue, configAck)) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: C&C Config Update: Failed to send Configuration Ack Message", __func__);
        configAck->destroy(configAck);
        return false;
    }
    configAck->destroy(configAck);
    return true;
}

static bool
Default_processConfAckMessage (struct Message *message) {
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }
    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Conf Ack", __func__);
    return true;
}

static bool
Default_processConfErrMessage (struct Message *message) {
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }
    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Conf Err", __func__);
    return true;
}

static bool
Default_processPauseMessage (struct Message *message) {
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }
    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Pause", __func__);
    return true;
}

static bool
Default_processPausedMessage (struct Message *message) {
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }
    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Paused", __func__);
    return true;
}

static bool
Default_processGoMessage (struct Message *message) {
    struct RazorbackContext *l_pContext;
    struct Message *running;
    uuid_t source,dest;
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }

    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Go", __func__);

    if ((l_pContext = Thread_GetContext(sg_tThread)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Recieved go message for unknown context", __func__);
        return false;
    }

    // Wait up the registering thread.
    l_pContext->regOk = true;
    Semaphore_Post(l_pContext->regSem);

    Message_Get_Nuggets(message, source, dest);

    if ((running = MessageRunning_Initialize(dest, source)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to create running message", __func__);
        return false;
    }
    if (!Queue_Put(sg_writeQueue, running)) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Go Hook: Failed to send running message", __func__);
        running->destroy(running);
        return false;
    }
    running->destroy(running);
    return true;
}

static bool
Default_processRunningMessage (struct Message *message) {
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }
    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Running", __func__);
    return true;
}

static bool
Default_processTermMessage (struct Message *message) {
    struct MessageTerminate *term;
    struct Message *bye;
    struct RazorbackContext *context;
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }

    term = message->message;

    rzb_log(LOG_INFO, LOG_C_CNC, "%s: Termination Requested: %s", __func__, term->sTerminateReason);
    context = Thread_GetContext(sg_tThread);

    if ((bye = MessageBye_Initialize(context->uuidNuggetId)) == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Terminate Hook: Failed to alloc bye message", __func__);
    } else {
        if (!Queue_Put(sg_writeQueue, bye)) {
            rzb_log(LOG_ERR, LOG_C_CNC, "%s: Terminate Hook: Failed to send bye message", __func__);
        }
        bye->destroy(bye);
    }
    exit(0); // TODO: This should be a clean shutdown
    return true;
}

static bool
Default_processByeMessage (struct Message *message) {
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }
    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Bye", __func__);
    return true;
}

static void
ReregistrationThread(Thread_t *thread) {
    struct RazorbackContext *context;
    ASSERT (thread != NULL);
    if (thread == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to process re-registration, thread NULL", __func__);
        return;
    }

    context = Thread_GetContext(thread);
    ASSERT (context != NULL);
    if (context == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to process re-registration, context NULL", __func__);
        return;
    }

    context->regOk = false;

    if (!CommandAndControl_Register(context)) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Failed to re-register context, destroying it", __func__);
        Razorback_Destroy_Context(context);
        return;
    }

    return;
}

static bool
Default_processReRegMessage (struct Message *message) {
    struct RazorbackContext *context;
    Thread_t *thread;
    ASSERT(message != NULL);
    if (message == NULL) {
        rzb_log(LOG_ERR, LOG_C_CNC, "%s: Message is NULL", __func__);
        return false;
    }
    rzb_log (LOG_DEBUG, LOG_C_CNC, "%s: C&C Default Hook: Re-Register", __func__);
    context = Thread_GetContext(sg_tThread);
    thread = Thread_Launch(ReregistrationThread,NULL, NULL, context);
    //Decrement the ref count so it can be free'd once its finished.
    Thread_Destroy(thread);
    return true;
}
