#include "config.h"
#include <razorback/debug.h>
#include <razorback/log.h>
#include <razorback/thread.h>
#include <razorback/queue.h>
#include <razorback/judgment_queue.h>
#include <razorback/judgment.h>

#include "command_and_control.h"
#include "judgment_private.h"

bool
Judgment_Init (void)
{
    return JudgmentQueue_Initialize (QUEUE_FLAG_SEND);
}

SO_PUBLIC bool
Judgment_Log (uint8_t p_iLevel, struct Block *p_pBlock,
                struct EventId *p_pEventId,
                struct NTLVList *p_pMetadata)
{
    struct MessageJudgmentSubmission l_mjsMessage;
    struct RazorbackContext *l_pContext;

    l_pContext = Thread_GetCurrentContext ();

    MessageJudgmentSubmission_Initialize (&l_mjsMessage, p_pBlock, p_pEventId,
                                          l_pContext->uuidNuggetId,
                                          l_pContext->uuidApplicationType,
                                          JUDGMENT_REASON_LOG, p_iLevel,
                                          0, 0, p_pMetadata);
    pthread_mutex_lock (&sg_mPauseLock);
    JudgmentQueue_Put (&l_mjsMessage);
    pthread_mutex_unlock (&sg_mPauseLock);
    MessageJudgmentSubmission_Destroy (&l_mjsMessage);

    return true;
}


SO_PUBLIC bool
Judgment_Render_Verdict (uint8_t p_iLevel, struct Block *p_pBlock,
                            struct EventId *p_pEventId,
                            struct NTLVList *p_pMetadata,
                            uint32_t p_iSfFlags, uint32_t p_iEntFlags)
{
    struct MessageJudgmentSubmission l_mjsMessage;
    struct RazorbackContext *l_pContext;

    l_pContext = Thread_GetCurrentContext ();

    MessageJudgmentSubmission_Initialize (&l_mjsMessage, p_pBlock, p_pEventId,
                                          l_pContext->uuidNuggetId,
                                          l_pContext->uuidApplicationType,
                                          JUDGMENT_REASON_ALERT, p_iLevel,
                                          p_iSfFlags, p_iEntFlags, p_pMetadata);
    pthread_mutex_lock (&sg_mPauseLock);
    JudgmentQueue_Put (&l_mjsMessage);
    pthread_mutex_unlock (&sg_mPauseLock);
    MessageJudgmentSubmission_Destroy (&l_mjsMessage);

    return true;
}
