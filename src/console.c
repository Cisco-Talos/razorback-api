
#include <razorback/socket.h>
#include <razorback/thread.h>
#include <razorback/ntlv.h>
#include <razorback/log.h>
#include <string.h>

static struct Socket * sg_pSocket;
static struct Thread * sg_pThread;

void Console_Accept_Thread(struct Thread *p_pThread);
void Console_Thread(struct Thread *p_pThread);

bool
Console_Start(struct RazorbackContext *p_pContext)
{
    if ((sg_pSocket = Socket_Listen ((const uint8_t *)"127.0.0.1", 6666)) == NULL)
    {
        rzb_log(LOG_ERR, "Console_Start: Failed to start listening socket");
        return false;
    }

    sg_pThread = Thread_Launch( Console_Accept_Thread, NULL, (char *)"Console Server", p_pContext) ;
    if (sg_pThread == NULL) 
    {
        rzb_log(LOG_ERR, "Console_Start: Failed to launch accept thread");
        return false;
    }
    return true;

}

void Console_Accept_Thread(struct Thread *p_pThread)
{
    while (true) 
    {
        struct Socket *l_pSocket;
        if (Socket_Accept(&l_pSocket, sg_pSocket) == 1) 
        {
            rzb_log(LOG_INFO, "Launch");
            Thread_Launch(Console_Thread, NULL, (char *)"Console Session", p_pThread->pContext);
        }
//        rzb_perror("Err/Timeout: %s");

    }
}

void Console_Thread(struct Thread *p_pThread)
{
    rzb_log(LOG_DEBUG, "Console_Thread: Launched");
    rzb_log(LOG_DEBUG, "Console_Thread: Launched");
    rzb_log(LOG_DEBUG, "Console_Thread: Launched");
    rzb_log(LOG_DEBUG, "Console_Thread: Launched");
    rzb_log(LOG_DEBUG, "Console_Thread: Launched");
    rzb_log(LOG_DEBUG, "Console_Thread: Launched");
    sleep(200);
}

