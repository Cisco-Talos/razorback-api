#ifndef RAZORBACK_EVENT_H
#define RAZORBACK_EVENT_H

#include <razorback/types.h>

extern struct EventId * EventId_Create (void);
extern struct EventId * EventId_Clone (struct EventId *event);
extern void EventId_Destroy (struct EventId *event);

extern struct Event * Event_Create (void);

extern void Event_Destroy (struct Event *event);

extern uint32_t Event_BinaryLength (struct Event *event);
#endif
