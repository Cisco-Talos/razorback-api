#ifndef	RAZORBACK_API_INTERNAL_H
#define	RAZORBACK_API_INTERNAL_h

#include <razorback/types.h>

extern bool
Razorback_ForEach_Context (int (*function) (struct RazorbackContext *, void *), void *userData);

#endif
