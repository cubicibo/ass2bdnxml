#include <stdint.h>

typedef struct BoundingBox_s {
    int x1;
    int x2;
    int y1;
    int y2;
} BoundingBox_t;

typedef struct frate_s {
    char *name;
    int rate;
    uint64_t num;
    uint64_t denom;
} frate_t;

typedef struct image_s {
    int width, height, stride, dvd_mode;
    int subx1, suby1, subx2, suby2;
    uint64_t in, out;
    BoundingBox_t crops[2];
    uint8_t *buffer;
} image_t;

typedef struct eventlist_s {
    int size, nmemb;
    image_t **events;
} eventlist_t;

typedef struct opts_s {
    double par;
    int64_t offset;
    int frame_w;
    int frame_h;
    int render_w;
    int render_h;
    int storage_w;
    int storage_h;
    uint16_t quantize;
    uint8_t dvd_mode;
    uint8_t hinting;
    uint8_t split;
    uint8_t rle_optimise;
    const char *fontdir;
} opts_t;

eventlist_t *render_subs(char *subfile, frate_t *frate, opts_t *args);
