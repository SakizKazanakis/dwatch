#include "monolith_design_resource.h"
/* The OEM background descriptor has raw words [6, 0, path].
 * Reuse these known bytes, replacing only the pointer to the old image file.
 * Not proof that the remote resource exists or that OEM loader accepts it.
 */
int monolith_attach_existing_design(void *owner, monolith_image_renderer renderer) {
    if (!owner || !renderer) return -1;
    const monolith_design_image_descriptor descriptor = {
        6u, 0u, MONOLITH_DESIGN_REMOTE_PATH
    };
    return renderer(owner, &descriptor);
}
