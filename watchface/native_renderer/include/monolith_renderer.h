#ifndef MONOLITH_NATIVE_RENDERER_H
#define MONOLITH_NATIVE_RENDERER_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { MONOLITH_WIDTH = 485, MONOLITH_HEIGHT = 520, MONOLITH_CHANNELS = 3 };
typedef struct monolith_renderer {
    uint8_t *framebuffer;
    const uint8_t *background;
    size_t capacity;
    uint8_t owns_memory;
    uint8_t initialized;
    uint8_t paused;
    uint8_t reserved;
    unsigned draw_count;
} monolith_renderer;
typedef enum { MONOLITH_OK = 0, MONOLITH_BAD_ARGUMENT = -1,
               MONOLITH_NO_MEMORY = -2, MONOLITH_BAD_STATE = -3 } monolith_result;
/* Background is borrowed immutable RGB888 produced from verified eZIP. */
int monolith_renderer_init(monolith_renderer *r,const uint8_t *rgb,size_t rgb_size);
int monolith_renderer_draw(monolith_renderer *r,unsigned hour,unsigned minute);
int monolith_renderer_pause(monolith_renderer *r);
int monolith_renderer_resume(monolith_renderer *r);
void monolith_renderer_deinit(monolith_renderer *r);
int monolith_image_create(monolith_renderer *r,const uint8_t *rgb,size_t length);
int monolith_resource_decode(const uint8_t *resource,size_t length,
                             uint8_t *output,size_t output_size);
/* Low-memory shared-Huffman decoder: 32 KiB LZ77 history, caller-owned RGB
 * output. Validated separately; does not decode to arbitrary firmware VRAM. */
int monolith_resource_decode_lowmem(const uint8_t *resource,size_t length,
                                    uint8_t *output,size_t output_size);
/* Both native C decoders support only the verified original
 * 485x520 legacy shared-Huffman eZIP variant. */
const uint8_t *monolith_renderer_frame(const monolith_renderer *r);
size_t monolith_renderer_allocated(const monolith_renderer *r);
#ifdef __cplusplus
}
#endif
#endif
