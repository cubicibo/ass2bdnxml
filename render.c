#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ass/ass.h>
#include <png.h>

#include "common.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))

ASS_Library *ass_library;
ASS_Renderer *ass_renderer;

static image_t *image_init(int width, int height, int dvd_mode)
{
    image_t *img = calloc(1, sizeof(image_t));
    img->width = width;
    img->height = height;
    img->dvd_mode = dvd_mode;
    img->subx1 = img->suby1 = -1;
    img->stride = width * 4;
    img->buffer = calloc(1, height * width * 4);
    return img;
}

static void image_reset(image_t *img)
{
    img->subx1 = img->suby1 = -1;
    img->subx2 = img->suby2 = 0;
    img->buffer = memset(img->buffer, 0, img->height * img->stride);
}

void eventlist_set(eventlist_t *list, image_t *ev, int index)
{
    image_t *newev;

    if (list->nmemb <= index)
        newev = calloc(1, sizeof(image_t));
    else
        newev = list->events[index];

    newev->subx1 = ev->subx1;
    newev->subx2 = ev->subx2;
    newev->suby1 = ev->suby1;
    newev->suby2 = ev->suby2;
    newev->in = ev->in;
    newev->out = ev->out;

    if (list->size <= index) {
        list->size = index + 200;
        list->events = realloc(list->events, sizeof(image_t*) * list->size);
    }

    list->events[index] = newev;
    list->nmemb = MAX(list->nmemb, index + 1);
}

void eventlist_free(eventlist_t *list)
{
    int i;

    for (i = 0; i < list->nmemb; i++) {
        free(list->events[i]);
    }

    free(list->events);
    free(list);
}

static void msg_callback(int level, const char *fmt, va_list va, void *data)
{
    if (level > 6)
        return;
    printf("libass: ");
    vprintf(fmt, va);
    printf("\n");
}

static void write_png(char *fname, image_t *img)
{
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;
    png_byte **row_pointers;
    int k, w, h;
    uint8_t *img_s;

    img_s = img->buffer + img->suby1 * img->stride;
    w = img->subx2 - img->subx1 + 1;
    h = img->suby2 - img->suby1 + 1;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                      NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);
    fp = NULL;

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return;
    }

    fp = fopen(fname, "wb");
    if (fp == NULL) {
        printf("PNG Error opening %s for writing!\n", fname);
        return;
    }

    png_init_io(png_ptr, fp);
    png_set_compression_level(png_ptr, 3);

    png_set_IHDR(png_ptr, info_ptr, w, h, 8,
                 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    png_set_bgr(png_ptr);

    row_pointers = (png_byte **) malloc(h * sizeof(png_byte *));

    for (k = 0; k < h; k++) {
        row_pointers[k] = img_s + img->stride * k + img->subx1 * 4;
    }

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    free(row_pointers);

    fclose(fp);
}

static void init(opts_t *args)
{
    ass_library = ass_library_init();
    if (!ass_library) {
        printf("ass_library_init failed!\n");
        exit(1);
    }

    ass_set_message_cb(ass_library, msg_callback, NULL);
    ass_set_extract_fonts(ass_library, 1);

    ass_renderer = ass_renderer_init(ass_library);
    if (!ass_renderer) {
        printf("ass_renderer_init failed!\n");
        exit(1);
    }
    ass_set_frame_size(ass_renderer, args->render_w, args->render_h);
    ass_set_pixel_aspect(ass_renderer, args->par);
    
    ass_set_fonts(ass_renderer, NULL, "sans-serif",
                  ASS_FONTPROVIDER_AUTODETECT, NULL, 1);

    if (args->hinting >= 0 && args->hinting <= ASS_HINTING_NATIVE) {
        ass_set_hinting(ass_renderer, (ASS_Hinting)args->hinting);
    } else {
        printf("Incorrect hinting value.\n");
        exit(1);
    }
}

#define _r(c)  ((c)>>24)
#define _g(c)  (((c)>>16)&0xFF)
#define _b(c)  (((c)>>8)&0xFF)
#define _a(c)  ((c)&0xFF)

#define ablend(srcA, srcRGB, dstA, dstRGB, outA) \
    (((srcA * 255 * srcRGB + (dstRGB * dstA * (255 - srcA))) / outA) / 255)

static void blend_single(image_t * frame, ASS_Image *img)
{
    int x, y, c;
    uint8_t opacity = 255 - _a(img->color);
    uint8_t r = _r(img->color);
    uint8_t g = _g(img->color);
    uint8_t b = _b(img->color);

    uint8_t *src;
    uint8_t *dst;

    src = img->bitmap;
    dst = frame->buffer + img->dst_y * frame->stride + img->dst_x * 4;

    for (y = 0; y < img->h; y++) {
        for (x = 0, c = 0; x < img->w; x++, c += 4) {
            unsigned k = (src[x] * opacity) / 255;

            if (frame->dvd_mode) {
                if (dst[c+3])
                    k = (MIN((int)(255 * pow((k / 255.0), 1 / 0.75) * 1.2),
                             255) / 127) * 127;
                else
                    k = k > 164 ? 255 : 0;
            }

            if (k) {
                uint8_t outa = (k * 255 + (dst[c + 3] * (255 - k))) / 255;

                dst[c    ] = ablend(k, b, dst[c + 3], dst[c    ], outa);
                dst[c + 1] = ablend(k, g, dst[c + 3], dst[c + 1], outa);
                dst[c + 2] = ablend(k, r, dst[c + 3], dst[c + 2], outa);
                dst[c + 3] = outa;
            }
        }

        src += img->stride;
        dst += frame->stride;
    }
}

static void blend(image_t *frame, ASS_Image *img)
{
    int x, y, c;
    uint8_t *buf = frame->buffer;

    image_reset(frame);

    while (img) {
        blend_single(frame, img);
        img = img->next;
    }

    for (y = 0; y < frame->height; y++) {
        for (x = 0, c = 0; x < frame->width; x++, c += 4) {
            uint8_t k = buf[c + 3];

            if (k) {
                /* Some DVD and BD players need the offsets to be on mod2
                 * positions and will misrender subtitles or crash if they
                 * are not. Yeah, really. */
                if (frame->subx1 < 0) frame->subx1 = x - (x % 2);
                else frame->subx1 = MIN(frame->subx1, x - (x % 2));

                if (frame->suby1 < 0) frame->suby1 = y - (y % 2);
                else frame->suby1 = MIN(frame->suby1, y - (y % 2));

                frame->subx2 = MAX(frame->subx2, x);
                frame->suby2 = MAX(frame->suby2, y);
            }
        }

        buf += frame->stride;
    }
}

static int get_frame(ASS_Renderer *renderer, ASS_Track *track, image_t *frame,
                     long long time, int frame_d)
{
    int changed;
    ASS_Image *img = ass_render_frame(renderer, track, time, &changed);

    if (changed && img) {
        frame->out = time + frame_d;
        blend(frame, img);
        frame->in = time;

        if (frame->subx1 == -1 || frame->suby1 == -1)
            return 2;

        return 3;
    } else if (!changed && img) {
        frame->out = time + frame_d;
        return 1;
    } else {
        return 0;
    }
}

eventlist_t *render_subs(char *subfile, int frame_d, opts_t *args)
{
    long long tm = 0;
    int count = 0, fres = 0;
    eventlist_t *evlist = calloc(1, sizeof(eventlist_t));

    init(args);
    ASS_Track *track = ass_read_file(ass_library, subfile, NULL);

    if (!track) {
        printf("track init failed!\n");
        exit(1);
    }

    image_t *frame = image_init(args->frame_w, args->frame_h, args->dvd_mode);

    while (1) {
        if (fres && fres != 2 && count) {
            eventlist_set(evlist, frame, count - 1);
        }

        fres = get_frame(ass_renderer, track, frame, tm, frame_d);

        switch (fres) {
            case 3:
            {
                char imgfile[13];

                snprintf(imgfile, 13, "%08d.png", count);
                write_png(imgfile, frame);

                count++;
            }
            /* fall through */
            case 2:
            case 1:
                tm += frame_d;
                break;
            case 0:
            {
                long long offset = ass_step_sub(track, tm, 1);

                if (tm && !offset)
                    goto finish;

                tm += offset;
                break;
            }
        }
    }

finish:
    free(frame->buffer);
    free(frame);
    ass_free_track(track);
    ass_renderer_done(ass_renderer);
    ass_library_done(ass_library);

    return evlist;
}
