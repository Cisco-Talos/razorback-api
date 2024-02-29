/** @file thread.h
 * Threading API.
 */

#ifndef	RAZORBACK_THREAD_H
#define	RAZORBACK_THREAD_H

#include <razorback/types.h>
#include <razorback/api.h>
#include <pthread.h>


/** Thread
 * Purpose:	hold the information about a thread
 */
struct Thread
{
    pthread_t iThread;          ///< Pthread Thread info.
    pthread_mutex_t mMutex;  ///< mutex protecting this struct
    bool bRunning;              ///< true if executing, false if not:  must be managed explicitly by thread function
    void *pUserData;		///< Additional info for the thread
    char *sName;            ///< The thread name
    struct RazorbackContext *pContext; ///< The Thread Context
    void (*mainFunction) (struct Thread *); ///< Thread Main Function
};

/** Create a new thread
 * @param *p_fpFunction The function the thread will execute
 * @param *p_pUserData The thread user data
 * @return Null on error a new Thread on success.
 */
extern struct Thread *Thread_Launch (void (*p_fpFunction) (struct Thread *),
                                     void *p_pUserData, char *p_sName,
                                     struct RazorbackContext *p_pContext);

/** Change the registered context of a running thread.
 * @param p_pThread the thread to change
 * @param p_pContext the new context
 * @return The old context
 */
extern struct RazorbackContext * Thread_ChangeContext(struct Thread *p_pThread,
                                    struct RazorbackContext *p_pContext);

/** Get the registered context of a running thread.
 * @param p_pThread the thread to change
 * @return The context
 */
extern struct RazorbackContext * Thread_GetContext(struct Thread *p_pThread);
extern struct RazorbackContext * Thread_GetCurrentContext(void);

/** Destroy a threads data
 * @param *p_pThread The thread to destroy
 */
extern void Thread_Destroy (struct Thread *p_pThread);

/** Checks whether a thread is running or not
 * @param *p_pThread The thread the test
 * @return true if running, false if not
 */
extern bool Thread_IsRunning (struct Thread *p_pThread);

/** Get the number of currently running threads.
 * @return the number of currently running threads.
 */
extern uint32_t Thread_getCount (void);

/** Get the current thread.
 */
extern struct Thread *Thread_GetCurrent(void);


#endif // RAZORBACK_THREAD_H
