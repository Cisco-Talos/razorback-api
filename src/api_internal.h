#ifndef	RAZORBACK_API_INTERNAL_H
#define	RAZORBACK_API_INTERNAL_h

#include <razorback/types.h>
#ifdef __cplusplus
extern "C" {
#endif
extern bool
Razorback_ForEach_Context (int (*function) (struct RazorbackContext *, void *), void *userData);
#ifdef __cplusplus
}
#endif
#endif
