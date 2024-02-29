#ifndef RAZORBACK_METADATA_H
#define RAZORBACK_METADATA_H

#include <razorback/types.h>

#define Metadata_Add NTLVList_Add
//extern bool Metadata_Add (struct NTLVList *list, uuid_t name, uuid_t type, uint32_t size, uint8_t *data);

extern bool Metadata_Add_String (struct NTLVList *list, uuid_t name, const char *string);

extern bool Metadata_Add_IPv4 (struct NTLVList *list, uuid_t name, uint8_t *addr);
extern bool Metadata_Add_IPv6 (struct NTLVList *list, uuid_t name, uint8_t *addr);
extern bool Metadata_Add_Port (struct NTLVList *list, uuid_t name, uint8_t *port);


extern bool Metadata_Add_Filename (struct NTLVList *list, const char *name);
extern bool Metadata_Add_Hostname (struct NTLVList *list, const char *name);
extern bool Metadata_Add_MalwareName (struct NTLVList *list, const char *name);


extern bool Metadata_Add_IPv4_Source (struct NTLVList *list, uint8_t *addr);
extern bool Metadata_Add_IPv4_Destination (struct NTLVList *list, uint8_t *addr);
extern bool Metadata_Add_IPv6_Source (struct NTLVList *list, uint8_t *addr);
extern bool Metadata_Add_IPv6_Destination (struct NTLVList *list, uint8_t *addr);
extern bool Metadata_Add_Port_Source (struct NTLVList *list, uint8_t *port);
extern bool Metadata_Add_Port_Destintation (struct NTLVList *list, uint8_t *port);

#endif
