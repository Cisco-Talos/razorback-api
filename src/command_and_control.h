#ifndef RAZORBACK_COMMAND_AND_CONTROL_H
#define RAZORBACK_COMMAND_AND_CONTROL_H
#include <razorback/types.h>
#include <razorback/api.h>
#include <pthread.h>

extern bool CommandAndControl_Start (struct RazorbackContext *p_pContext);

extern pthread_mutex_t sg_mPauseLock;

#endif
