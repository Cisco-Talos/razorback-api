#ifndef RAZORBACK_COMMAND_AND_CONTROL_H
#define RAZORBACK_COMMAND_AND_CONTROL_H
#include <razorback/types.h>
#include <razorback/api.h>
#include <pthread.h>

extern bool CommandAndControl_Start (struct RazorbackContext *p_pContext);

extern bool CommandAndControl_SendBye (struct RazorbackContext *context);
extern pthread_mutex_t sg_mPauseLock;

extern void CommandAndControl_Pause();
extern void CommandAndControl_Unpause();


#endif
