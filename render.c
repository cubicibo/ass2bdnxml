#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ass/ass.h>
#include <png.h>
#include <libimagequant.h>

#include "common.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define BOX_AREA(box) ((box.x2-box.x1)*(box.y2-box.y1))

typedef enum SamplingFlag_s {
    SAMPLE_TC_IN  = 0,
    SAMPLE_TC_OUT,
    SAMPLE_TC_MID,
    INVALID_SAMPLING
} SamplingFlag_t;

ASS_Library *ass_library;
ASS_Renderer *ass_renderer;
liq_attr *attr;

static image_t *image_init(int width, int height, int dvd_mode)
{
    image_t *img = calloc(1, sizeof(image_t));
    img->width = width;
    img->height = height;
    img->dvd_mode = dvd_mode;
    img->subx1 = img->suby1 = -1;
    img->stride = width * 4;
    img->buffer = calloc(1, height * width * 4);
    memset(img->crops, 0xFF, sizeof(BoundingBox_t)*2);
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
    memcpy(newev->crops, ev->crops, sizeof(BoundingBox_t)*2);

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

static void write_png_palette(char *fname, image_t *rgba_img, liq_image **img, liq_result **res)
{
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;

    int k, w, h;

    w = rgba_img->subx2 - rgba_img->subx1 + 1;
    h = rgba_img->suby2 - rgba_img->suby1 + 1;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info_ptr = png_create_info_struct(png_ptr);

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        printf("Critical error in libpng while processing %s.\n", fname);
        return;
    }

    fp = fopen(fname, "wb");
    if (fp == NULL) {
        printf("PNG Error opening %s for writing!\n", fname);
        return;
    }

    const liq_palette *liq_pal = liq_get_palette(*res);
    png_color* palette = (png_color*)png_malloc(png_ptr, liq_pal->count*sizeof(png_color));
    png_byte* trans = (png_byte*)png_malloc(png_ptr, liq_pal->count);

    uint8_t* bitmap = (uint8_t*)malloc(rgba_img->width*rgba_img->height);
    liq_write_remapped_image(*res, *img, (void*)bitmap, rgba_img->width*rgba_img->height);

    for (k = 0; k < liq_pal->count; k++)
    {
        png_color* col = &palette[k];
        //palettized as BGR, flip B and R.
        col->red = liq_pal->entries[k].b;
        col->green = liq_pal->entries[k].g;
        col->blue = liq_pal->entries[k].r;
        trans[k] = liq_pal->entries[k].a;
    }

    png_init_io(png_ptr, fp);
    png_set_compression_level(png_ptr, 3);

    png_set_IHDR(png_ptr, info_ptr, w, h, 8,
                 PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    //Write palette and alpha
    png_set_PLTE(png_ptr, info_ptr, palette, liq_pal->count);
    png_set_tRNS(png_ptr, info_ptr, trans, liq_pal->count, NULL);
    png_write_info(png_ptr, info_ptr);

    png_byte *row;
    for (int k = 0; k < h; k++) {
        row = (png_byte*)(bitmap + (k + rgba_img->suby1)*rgba_img->width + rgba_img->subx1);
        png_write_row(png_ptr, row);
    }
    png_write_end(png_ptr, NULL);

    png_free(png_ptr, trans);
    png_free(png_ptr, palette);
    png_destroy_write_struct(&png_ptr, &info_ptr);
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

    // fonts stuff
    ass_set_extract_fonts(ass_library, 1);
    if (args->fontdir) {
        ass_set_fonts_dir(ass_library, args->fontdir);
    }

    ass_renderer = ass_renderer_init(ass_library);
    if (!ass_renderer) {
        printf("ass_renderer_init failed!\n");
        exit(1);
    }
    ass_set_frame_size(ass_renderer, args->render_w, args->render_h);
    if (args->par > 0) {
        ass_set_pixel_aspect(ass_renderer, args->par);
    }
    if (args->storage_w && args->storage_h) {
        ass_set_storage_size(ass_renderer, args->storage_w, args->storage_h);
    }

    ass_set_fonts(ass_renderer, NULL, "sans-serif",
                  ASS_FONTPROVIDER_AUTODETECT, NULL, 1);

    if (args->hinting >= 0 && args->hinting <= ASS_HINTING_NATIVE) {
        ass_set_hinting(ass_renderer, (ASS_Hinting)args->hinting);
    } else {
        printf("Incorrect hinting value.\n");
        exit(1);
    }

    if (args->quantize) {
        attr = liq_attr_create();
        if (attr == NULL) {
            printf("Failed to initialise libimagequant.\n");
            exit(1);
        }
        liq_set_max_colors(attr, args->quantize);
    }
}

#define _r(c)  ((c)>>24)
#define _g(c)  (((c)>>16)&0xFF)
#define _b(c)  (((c)>>8)&0xFF)
#define _a(c)  ((c)&0xFF)

#define div256(i) ((i+128) >> 8)
#define div255(i) (div256(i + div256(i)))

#define ablend(srcA, srcRGB, dstA, dstRGB, outA) \
    (((srcA * 255 * srcRGB + dstRGB * dstA * (255 - srcA) + (outA >> 1)) / outA))

static void blend_single(image_t * frame, ASS_Image *img)
{
    int x, y, c;
    uint32_t outa, k;
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
            k = MIN(div255(src[x] * opacity), 255);

            if (frame->dvd_mode) {
                if (dst[c+3])
                    k = (MIN((int)(255 * pow((k / 255.0), 1 / 0.75) * 1.2),
                             255) / 127) * 127;
                else
                    k = k > 164 ? 255 : 0;
            }

            if (k) {
                if (dst[c+3]) {
                    outa = (k * 255 + (dst[c + 3] * (255 - k)));

                    dst[c    ] = MIN(255, ablend(k, b, dst[c + 3], dst[c    ], outa));
                    dst[c + 1] = MIN(255, ablend(k, g, dst[c + 3], dst[c + 1], outa));
                    dst[c + 2] = MIN(255, ablend(k, r, dst[c + 3], dst[c + 2], outa));
                    dst[c + 3] = MIN(255, div255(outa));
                } else {
                    dst[c    ] = b;
                    dst[c + 1] = g;
                    dst[c + 2] = r;
                    dst[c + 3] = k;
                }
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

static void find_bbox(image_t *frame, int y_start, int y_stop, const int margin, BoundingBox_t *box)
{
    int pixelExist;
    int xk, yk;

    //left
    pixelExist = 0;
    for (xk = frame->subx1; xk < frame->subx2 - margin && !pixelExist; xk++) {
        for (yk = y_start; yk < y_stop; yk++) {
            pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3] > 0;
        }
    }
    box->x1 = MAX(xk-1, frame->subx1);

    //right
    pixelExist = 0;
    for (xk = frame->subx2; xk > frame->subx1 + margin && !pixelExist; xk--) {
        for (yk = y_start; yk < y_stop; yk++) {
            pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3] > 0;
        }
    }
    box->x2 = MIN(xk+1, frame->subx2);

    if (y_start == frame->suby1) {
        box->y1 = frame->suby1;

        pixelExist = 0;
        for (yk = y_stop; yk > y_start + margin && !pixelExist; yk--) {
            for (xk = box->x1; xk <= box->x2; xk++) {
                pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3] > 0;
            }
        }
        box->y2 = MIN(yk+1, y_stop);
    } else {
        box->y2 = frame->suby2;

        pixelExist = 0;
        for (yk = y_start; yk < y_stop - margin && !pixelExist; yk++) {
            for (xk = box->x1; xk <= box->x2; xk++) {
                pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3] > 0;
            }
        }
        box->y1 = MAX(yk-1, y_start);
    }
}

static int find_split(image_t *frame)
{
    const int margin = 8;
    uint32_t best_score = (uint32_t)(-1);
    uint8_t pixelExist;

    int yk, xk;
    memset(frame->crops, 0xFF, sizeof(BoundingBox_t)*2);

    //Prevent splits smaller than 8 pixels
    if (frame->suby2 - frame->suby1 < margin*2) {
        return 0;
    }

    BoundingBox_t eval[2];
    uint32_t surface = 0;

    for (yk = frame->suby1 + margin; yk < frame->suby2 - margin; yk+=margin) {
        pixelExist = 0;
        for (xk = frame->subx1; xk < frame->subx2 && !pixelExist; xk++) {
            pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3];
        }

        //Line is used by data, skip to the next split.
        if (pixelExist) {
            continue;
        }

        find_bbox(frame, frame->suby1, yk, margin, &eval[0]);
        find_bbox(frame, yk, frame->suby2, margin, &eval[1]);

        surface = BOX_AREA(eval[0]) + BOX_AREA(eval[1]);
        if (surface < best_score) {
            best_score = surface;
            memcpy(frame->crops, eval, sizeof(eval));
        }
    }
    return best_score < (uint32_t)(-1);
}

static uint64_t frame_to_realtime_ms(uint64_t frame_cnt, frate_t *frate, SamplingFlag_t flag)
{
    if (flag == SAMPLE_TC_OUT) {
        return (uint64_t)floor((1000 * frame_cnt * frate->denom)/(double)frate->num);
    } else if (flag == SAMPLE_TC_IN) {
        return (uint64_t)ceil((1000*(frame_cnt - 1) * frate->denom)/(double)frate->num);
    } else if (flag == SAMPLE_TC_MID) {
        return (uint64_t)round(((1000*frame_cnt * frate->denom)/frate->num) - (500*frate->denom)/frate->num);
    }
    fprintf(stderr, "Invalid sampling flag.\n");
    exit(1);
}

static int get_frame(ASS_Renderer *renderer, ASS_Track *track,
                     image_t *frame, uint64_t frame_cnt, frate_t *frate)
{
    int changed;

    uint64_t ms = frame_to_realtime_ms(frame_cnt, frate, SAMPLE_TC_MID);
    ASS_Image *img = ass_render_frame(renderer, track, ms, &changed);

    if (changed && img) {
        frame->out = frame_cnt + 1;
        blend(frame, img);
        frame->in = frame_cnt;

        if (frame->subx1 == -1 || frame->suby1 == -1)
            return 2;

        return 3;
    } else if (!changed && img) {
        ++frame->out;
        return 1;
    } else {
        return 0;
    }
}

static int quantize_event(image_t *frame, liq_image **img, liq_result **qtz_res)
{
    *img = liq_image_create_rgba(attr, /*&*/frame->buffer/*[frame->stride*frame->y1]*/, frame->width, frame->height, 0);
    if (*img == NULL) {
        return -1;
    }

    if(liq_image_quantize(*img, attr, qtz_res) != LIQ_OK) {
        return -1;
    }
    return 0;
}

eventlist_t *render_subs(char *subfile, frate_t *frate, opts_t *args)
{
    long long tm = 0;
    int count = 0, fres = 0, img_cnt = 0;
    uint64_t frame_cnt = 1;

    liq_result *res;
    liq_image *img;

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

        fres = get_frame(ass_renderer, track, frame, frame_cnt, frate);

        switch (fres) {
            case 3:
            {
                char imgfile[15];
                if (args->quantize && quantize_event(frame, &img, &res)) {
                    printf("Quantization failed for %08d.png.\n", count);
                    exit(1);
                }
                if (args->split && find_split(frame)) {
                    for (img_cnt = 0; img_cnt < 2; img_cnt++) {
                        frame->subx1 = frame->crops[img_cnt].x1;
                        frame->subx2 = frame->crops[img_cnt].x2;
                        frame->suby1 = frame->crops[img_cnt].y1;
                        frame->suby2 = frame->crops[img_cnt].y2;
                        snprintf(imgfile, 15, "%08d_%d.png", count, img_cnt);
                        if (args->quantize) {
                            write_png_palette(imgfile, frame, &img, &res);
                        } else {
                            write_png(imgfile, frame);
                        }
                    }
                } else {
                    snprintf(imgfile, 15, "%08d.png", count);
                    if (args->quantize) {
                        write_png_palette(imgfile, frame, &img, &res);
                    } else {
                        write_png(imgfile, frame);
                    }
                }
                if (args->quantize) {
                    liq_result_destroy(res);
                    liq_image_destroy(img);
                }
                count++;
            }
            /* fall through */
            case 2:
            case 1:
                ++frame_cnt;
                break;
            case 0:
            {
                tm = (uint64_t)ass_step_sub(track, frame_to_realtime_ms(frame_cnt, frate, SAMPLE_TC_MID), 1);
                uint64_t offset = (tm*frate->rate)/1000;

                if (!tm && frame_cnt > 1)
                    goto finish;

                if (offset == 0) {
                    offset = 1; //avoid deadlocks
                }

                frame_cnt += offset;
                break;
            }
        }
    }

finish:
    free(frame->buffer);
    free(frame);
    if (args->quantize && attr)
        liq_attr_destroy(attr);
    ass_free_track(track);
    ass_renderer_done(ass_renderer);
    ass_library_done(ass_library);

    return evlist;
}
