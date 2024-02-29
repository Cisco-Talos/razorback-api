#ifndef RZB_ALERT_UTIL_H
#define RZB_ALERT_UTIL_H

#include <sys/types.h>

void md5sum(const void *content, ssize_t len, unsigned char *md5);
char * md5sum_string(const void *content, ssize_t len);
const unsigned char *file_type_lookup(const void *data, size_t len);

#endif /* RZB_ALERT_UTIL_H */
