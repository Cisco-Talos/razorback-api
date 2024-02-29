/** @file judgment.h
 * Judgment transmission functions.
 */
#ifndef RAZORBACK_JUDGMENT_H
#define RAZORBACK_JUDGMENT_H

#include <razorback/types.h>

extern struct Judgment * Judgment_Create (struct EventId *eventId);
extern void Judgment_Destroy (struct Judgment *judgment);
extern uint32_t Judgment_BinaryLength (struct Judgment *judgment);

/** Render a verdict on a block
 */
extern bool
Judgment_Render_Verdict (uint8_t p_iLevel, struct Judgment *p_pJudgment);

#endif
