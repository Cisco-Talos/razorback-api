#ifndef RAZORBACK_JUDGMENT_H
#define RAZORBACK_JUDGMENT_H

#include <razorback/types.h>


/** Log information about a block
 */
extern bool
Judgment_Log (uint8_t p_iLevel, struct Block *p_pBlock,
                struct EventId *p_pEventId,
                struct NTLVList *p_pMetadata);

/** Render a verdict on a block
 */
extern bool
Judgment_Render_Verdict (uint8_t p_iLevel, struct Block *p_pBlock,
                            struct EventId *p_pEventId,
                            struct NTLVList *p_pMetadata,
                            uint32_t p_iSfFlags, uint32_t p_iEntFlags);

#endif
