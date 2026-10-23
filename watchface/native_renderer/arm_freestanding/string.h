/* OFFLINE ARM build ONLY. Freestanding C memory API declarations.
 * Does not imply that the stock HK8 ROM exports these names.
 */
#ifndef MONOLITH_ARM_STRING_H
#define MONOLITH_ARM_STRING_H
#include <stddef.h>
void *memcpy(void *,const void *,size_t);
void *memset(void *,int,size_t);
int memcmp(const void *,const void *,size_t);
#endif
