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

/** @file api.h
 * Razorback API.
 */
#ifndef RAZORBACK_API_H
#define RAZORBACK_API_H

#if defined(__cplusplus)
#if defined(_MSVC_LANG)
#if _MSVC_LANG < 202100L
#error "razorback/api.h requires C++23 or later"
#endif
#elif __cplusplus < 202100L
#error "razorback/api.h requires C++23 or later"
#endif
#endif

#include <stdatomic.h>
#include <razorback/visibility.h>
#include <razorback/types.h>
#include <razorback/queue.h>
#include <razorback/message_formats.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DECL_INSPECTION_FUNC(a) uint8_t a (struct Block *block, struct EventId *eventId, List_t *eventMetadata, void *threadData)
#define DECL_NUGGET_INIT bool initNug(void)
#define DECL_NUGGET_THREAD_INIT(a) bool a (void ** threadData)
#define DECL_NUGGET_THREAD_CLEANUP(a) void a (void * threadData)
#define DECL_NUGGET_SHUTDOWN void shutdownNug(void)
#define DECL_ALERT_PRIMARY_FUNC(a) bool a (struct MessageAlertPrimary *message)
#define DECL_ALERT_CHILD_FUNC(a) bool a (struct MessageAlertChild *message)
#define DECL_OUTPUT_EVENT_FUNC(a) bool a (struct MessageOutputEvent *message)

/**
 * Initialize process-wide Razorback library state.
 * This must be called exactly once by the library user before any other
 * Razorback API function is used. It is not invoked automatically by the
 * shared library loader.
 * @return No return value.
 */
SO_PUBLIC extern void RZB_Init_API(void);

/** Inspection Nugget Hooks
 */
struct RazorbackInspectionHooks
{
    uint8_t (*processBlock) (struct Block *, struct EventId *, List_t *, void *);  ///< FP to inspection handler
    bool (*processDeferredList) (struct DeferredList *);                           ///< FP to pending items
    bool (*initThread) (void  **);                                                 ///< FP to per thread init function
    void (*cleanupThread) (void *);                                                ///< FP to per thread cleanup function
};

/** Output nugget hooks
 */
struct RazorbackOutputHooks
{
    struct Queue *queue;                                          ///< reserved for in api use.
    const char *pattern;                                          ///<
    uint32_t messageType;                                         ///< Type of message requested.
    bool (*handleAlertPrimary)(struct MessageAlertPrimary *log);  ///< FP to handle primary alerts
    bool (*handleAlertChild)(struct MessageAlertChild *log);      ///< FP to handle child alerts
    bool (*handleEvent)(struct MessageOutputEvent *log);          ///< FP to handle events
};

/** Command and control hooks
 */
struct RazorbackCommandAndControlHooks
{
    bool (*processRegReqMessage) (struct Message *);      ///< Registration Request Handler
    bool (*processRegRespMessage) (struct Message *);     ///< Registration Response Handler
    bool (*processRegErrMessage) (struct Message *);      ///< Registration Error Handler
    bool (*processConfUpdateMessage) (struct Message *);  ///< Configuration Update Handler
    bool (*processConfAckMessage) (struct Message *);     ///< Configuration Acknowledgment Handler
    bool (*processConfErrMessage) (struct Message *);     ///< Configuration Error Handler
    bool (*processPauseMessage) (struct Message *);       ///< Pause Handler
    bool (*processPausedMessage) (struct Message *);      ///< Paused Handler
    bool (*processGoMessage) (struct Message *);          ///< Go Handler
    bool (*processRunningMessage) (struct Message *);     ///< Running Handler
    bool (*processTermMessage) (struct Message *);        ///< Terminate Handler
    bool (*processByeMessage) (struct Message *);         ///< Bye Handler
    bool (*processHelloMessage) (struct Message *);       ///< Hello Handler
    bool (*processReRegMessage) (struct Message *);       ///< Re-Registration Handler
};



#define CONTEXT_FLAG_STAND_ALONE 0x00000001
#define CONTEXT_FLAG_DEV_TOOL    0x00000002

/** API Context
 */
struct RazorbackContext
{
    uuid_t uuidNuggetId;                                    ///< Nugget UUID
    uuid_t uuidNuggetType;                                  ///< Nugget Type UUID
    uuid_t uuidApplicationType;                             ///< Nugget App Type UUID
    char *sNuggetName;                                      ///< Nugget Name
    uint32_t iFlags;                                        ///< Context Flags
    uint8_t locality;                                       ///< Nugget Locality
    struct RazorbackCommandAndControlHooks *pCommandHooks;  ///< Command And Control Hooks
    Semaphore_t *regSem;                                    ///< Registration semaphore
    bool regOk;                                             ///< Registration status
    void *userData;                                         ///< Context User Data
    atomic_bool paused;                                     ///< Whether work for this context is paused by C&C

    /** Inspector specific data.
     */
    struct Inspector
    {
        struct RazorbackInspectionHooks *hooks;  ///< Inspection Hooks
        uint32_t dataTypeCount;                  ///< Inspection Data Type Count
        uuid_t *dataTypeList;                    ///< Inspection Data Type UUID Array
        struct Queue *inspectionQueue;           ///< Shared inspector broker queue
        Thread_t *receiverThread;                ///< Broker receive and ack thread
        Thread_t *emergencyThread;               ///< Fatal inspection shutdown thread
        List_t *pendingMessages;                 ///< Internal inspection work queue
        List_t *completedMessages;               ///< Completed messages awaiting ack
        Mutex_t *emergencyLock;                  ///< Protects fatal inspection shutdown state
        Semaphore_t *emergencySem;               ///< Triggers fatal shutdown thread
        bool emergencyShutdownRequested;         ///< Whether fatal shutdown was requested
        atomic_bool shutdownStarted;             ///< Whether shutdown has stopped accepting new inspection work
        Mutex_t *workerInitLock;                 ///< Protects initial worker startup state
        Semaphore_t *workerInitSem;              ///< Barrier for initial worker startup
        uint32_t workerInitPending;              ///< Initial workers still reporting startup
        bool workerInitFailed;                   ///< Whether any worker failed initThread
        ThreadPool_t *threadPool;                ///< Inspection worker thread pool
        struct Queue *judgmentQueue;             ///< Judgment queue structure

    } inspector;

    /** Output specific data.
     */
    struct Output
    {
        List_t *threads;  ///< Output Thread List
    } output;

    /** Submission specific data.
     */
    struct Submission
    {
        ThreadPool_t *responseThreadPool;  ///< Per-context global-cache response thread pool
    } submission;

    /** Dispatcher specific data.
     */
    struct Dispatcher
    {
        uint32_t flags;       ///< Dispatcher Flags
        uint8_t priority;     ///< Dispatcher Priority
        uint16_t port;        ///< Dispatcher Transfer Port
        uint8_t protocol;     ///< Dispatcher Transfer Protocol
        List_t *addressList;  ///< Dispatcher Transfer Address List
    } dispatcher;
};

/**
 * Initialize an API context.
 * @param context The context to initialize.
 * @return true on success false on failure.
 */
SO_PUBLIC extern bool Razorback_Init_Context(struct RazorbackContext *context);

/**
 * Initialize an Inspection API context.
 * @param nuggetId the nugget uuid.
 * @param applicationType the application type.
 * @param dataTypeCount the number of data types.
 * @param dataTypeList the list of data types.
 * @param inspectionHooks the inspection call backs.
 * @return An initialized inspection context on success, NULL on failure.
 */
SO_PUBLIC extern struct RazorbackContext * Razorback_Init_Inspection_Context(
    uuid_t nuggetId,
    uuid_t applicationType,
    uint32_t dataTypeCount,
    uuid_t *dataTypeList,
    struct RazorbackInspectionHooks *inspectionHooks
);

/**
 * Initialize an Output Context.
 * @param nuggetId The nugget UUID.
 * @param applicationType The application type UUID.
 * @return An initialized output context on success, NULL on failure.
 */
SO_PUBLIC extern struct RazorbackContext * Razorback_Init_Output_Context(
    uuid_t nuggetId,
    uuid_t applicationType
);

/**
 * Initialize a Collection API context.
 * @param nuggetId the nugget uuid.
 * @param applicationType the application type.
 * @return An initialized output context on success, NULL on failure.
 */
SO_PUBLIC extern struct RazorbackContext * Razorback_Init_Collection_Context(
    uuid_t nuggetId,
    uuid_t applicationType
);

/**
 * Lookup a Context by UUID.
 * @param nuggetId The nugget ID uuid.
 * @return the context or NULL if there is no such context.
 */
SO_PUBLIC extern struct RazorbackContext * Razorback_LookupContext(uuid_t nuggetId);

/**
 * Shutdown a context.
 * @param context The context to shutdown.
 * @return No return value.
 */
SO_PUBLIC extern void Razorback_Shutdown_Context(struct RazorbackContext *context);

/**
 * Render a verdict on a block.
 * @param p_pJudgment The judgment information.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool Razorback_Render_Verdict(struct Judgment *p_pJudgment);

/**
 * Launch output threads.
 * @param context The output context.
 * @param hooks The output hook structure.
 * @return true on success, false on error.
 */
SO_PUBLIC extern bool Razorback_Output_Launch(
    struct RazorbackContext *context,
    struct RazorbackOutputHooks *hooks
);

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

#ifdef __cplusplus
}
#endif
#endif //RAZORBACK_API_H
