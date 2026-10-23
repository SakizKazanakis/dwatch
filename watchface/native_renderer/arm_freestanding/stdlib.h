/* OFFLINE ARM build ONLY. Freestanding declarations for imported allocator.
 * Not a firmware-compatible allocator; unresolved imports are deliberate.
 */
#ifndef MONOLITH_ARM_STDLIB_H
#define MONOLITH_ARM_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void free(void *);
#endif
