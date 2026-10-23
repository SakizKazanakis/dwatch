/* MONOLITH independent native SiFli eZIP decoder and RGB888 render API.
 * Not a firmware-matched OEM SDK, no BLE/OTA or physical-device access.
 */
#ifndef MONOLITH_NATIVE_RENDERER_H
#define MONOLITH_NATIVE_RENDERER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MONOLITH_IMG_MAX_W 2047u
#define MONOLITH_IMG_MAX_H 2047u
#define MONOLITH_RESOURCE_MAX_BYTES (32u*1024u*1024u)
#define MONOLITH_FORMAT_BGR888_EZIP 1u

typedef struct monolith_resource {
    const uint8_t *compressed;
    size_t compressed_bytes;
    uint16_t width, height;
    uint16_t row_height, block_count;
    uint8_t color_format, bit_depth;
    uint32_t first_block, table_end;
    uint8_t *rgb;
    size_t rgb_size;
    size_t peak_memory_bytes;
} monolith_resource;

typedef struct monolith_renderer {
    monolith_resource *image;
    uint8_t *canvas;
    size_t canvas_size;
    unsigned width, height;
    int active;
    unsigned last_minute;
    unsigned last_date;
    unsigned updates;
} monolith_renderer;

enum monolith_status {
    MONOLITH_OK=0,
    MONOLITH_INVALID=-1,
    MONOLITH_UNSUPPORTED=-2,
    MONOLITH_NOMEM=-3,
    MONOLITH_DECODE_FAILED=-4,
    MONOLITH_STATE=-5
};

int monolith_resource_open(monolith_resource *r,const uint8_t *data,size_t size);
int monolith_resource_decode(monolith_resource *r);
void monolith_resource_release(monolith_resource *r);
int monolith_renderer_init(monolith_renderer *ctx,monolith_resource *res);
int monolith_renderer_draw(monolith_renderer *ctx,unsigned hour,unsigned minute,
                           unsigned day,unsigned month,unsigned year);
int monolith_renderer_pause(monolith_renderer *ctx);
int monolith_renderer_resume(monolith_renderer *ctx);
void monolith_renderer_deinit(monolith_renderer *ctx);
const char *monolith_status_string(int status);
#ifdef __cplusplus
}
#endif
#endif
