/*
 * Copyright (c) 2011-2026 Cisco Systems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

/** @file hash.h
 * Hash functions
 */

#ifndef	RAZORBACK_HASH_H
#define	RAZORBACK_HASH_H

#include <razorback/visibility.h>
#include <razorback/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Determines whether to hashes are equal
 * @param p_pHashA the first hash
 * @param p_pHashB the second hash
 * @return true if equal, false otherwise
 */
SO_PUBLIC extern bool Hash_IsEqual (const struct Hash *p_pHashA,
                          const struct Hash *p_pHashB);

/** Converts a hash to text
 * @param p_pHash the hash
 * @return The string (requires free'ing) or NULL on error.
 */
SO_PUBLIC extern char * Hash_ToText (const struct Hash *p_pHash);

/** Create a hash
 * @return a new hash on success NULL on error.
 */
SO_PUBLIC extern struct Hash * Hash_Create (void);
SO_PUBLIC extern struct Hash * Hash_Create_Type (uint32_t p_iType);
SO_PUBLIC extern struct Hash * Hash_Create_From_String(uint32_t p_iType, const char *p_sHash);
/** Return the size of the digest in bytes.
 * @param p_pHash the hash.
 * @return the length in bytes.
 */
SO_PUBLIC extern uint32_t Hash_DigestLength (struct Hash *p_pHash);
//SO_PUBLIC extern uint32_t Hash_BinaryLength (struct Hash *p_pHash);

/** Return the size of the string required to hold the hex encoded digets.
 * @param p_pHash the hash.
 * @return the length of the array including null
 */
SO_PUBLIC extern uint32_t Hash_StringLength (struct Hash *p_pHash);

/** Update the hash with new data.
 * @param *p_pHash The hash to update.
 * @param *p_pData The data to compute the hash of.
 * @param *p_iLength The length of the data.
 * @return true on sucess false on error.
 */
SO_PUBLIC extern bool Hash_Update (struct Hash *p_pHash, uint8_t * p_pData,
                         uint32_t p_iLength);
SO_PUBLIC extern bool
Hash_Update_File (struct Hash * p_pHash, FILE *file);

/** Finalize the has value.
 * @param *p_pHash The hash object.
 * @return true on success false on error.
 */
SO_PUBLIC extern bool Hash_Finalize (struct Hash *p_pHash);

/** destroys a hash
 * @param p_pHash the hash
 */
SO_PUBLIC extern void Hash_Destroy (struct Hash *p_pHash);

/** copies a hash
 * @param p_pDestination the destination
 * @param p_pSource the source
 */
SO_PUBLIC extern struct Hash * Hash_Clone (const struct Hash *p_pSource);
#ifdef __cplusplus
}
#endif
#endif // RAZORBACK_HASH_H
