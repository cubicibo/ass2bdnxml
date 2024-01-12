#include <stdint.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define A2B_LOG_PREFIX "ass2bdnxml: "

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
    uint16_t splitmargin[2];
    uint8_t dvd_mode     : 1;
    uint8_t hinting      : 1;
    uint8_t split        : 1;
    uint8_t rle_optimise : 1;
    uint8_t keep_dupes   : 1;
    uint8_t anamorphic   : 1;
    uint8_t fullscreen   : 1;
    uint8_t square_px    : 1;
    const char *fontdir;
} opts_t;

typedef struct liqopts_s {
    float dither;
    uint8_t speed;
    uint8_t max_quality;
} liqopts_t;

eventlist_t *render_subs(char *subfile, frate_t *frate, opts_t *args, liqopts_t *liqargs);
