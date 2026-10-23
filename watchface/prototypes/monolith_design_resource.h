/* Monolith existing design -> OEM image descriptor adapter, OFFLINE ONLY.
 * The 3-word descriptor is directly recovered from the OEM image resource:
 * [6, 0, string-pointer]. Its C type/ownership is not verified.
 */
#ifndef MONOLITH_DESIGN_RESOURCE_H
#define MONOLITH_DESIGN_RESOURCE_H
#include <stdint.h>
#define MONOLITH_DESIGN_REMOTE_PATH "/ex/resource/monolith/clock_bg.bin"
#define MONOLITH_DESIGN_WIDTH 485
#define MONOLITH_DESIGN_HEIGHT 520
#define MONOLITH_OEM_ROOT_WIDTH 410
#define MONOLITH_OEM_ROOT_HEIGHT 494

typedef struct monolith_design_image_descriptor {
    uint32_t raw_type;
    uint32_t raw_zero;
    const char *resource_path;
} monolith_design_image_descriptor;

/* The pointer is 32-bit only in the target ARM module; host layout may be
 * 16 bytes on 64-bit. Never serialize the host struct to watch firmware.
 */
typedef int (*monolith_image_renderer)(void *owner,
    const monolith_design_image_descriptor *descriptor);

int monolith_attach_existing_design(void *owner, monolith_image_renderer renderer);
#endif
