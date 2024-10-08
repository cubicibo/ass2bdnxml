#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
#include <ass/ass.h>
#include <png.h>
#include <libimagequant.h>

#include "common.h"

#define FILENAME_FMT "%08d"
#define FILENAME_CNT "_%01d"
#define FILENAME_EXT ".png"
#define FILENAME_MAX_LENGTH (20)

#define BOX_AREA(box) ((box.x2-box.x1)*(box.y2-box.y1))

ASS_Library *ass_library;
ASS_Renderer *ass_renderer;
liq_attr *attr;

static image_t *image_init(int width, int height)
{
    image_t *img = calloc(1, sizeof(image_t));
    img->width = width;
    img->height = height;
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
        image_t **new_list = realloc(list->events, sizeof(image_t*) * list->size);
        if (!new_list) {
            printf("Can't allocate memory.\n");
            exit(1);
        }
        list->events = new_list;
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

static void write_png_palette(uint32_t count, image_t* restrict rgba_img, liq_image **img, liq_result **res, opts_t *args, uint8_t is_split, float dither_val)
{
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;

    int k, w, h;
    int h_margin, w_margin;
    char fname[FILENAME_MAX_LENGTH];

    w = rgba_img->subx2 - rgba_img->subx1 + 1;
    h = rgba_img->suby2 - rgba_img->suby1 + 1;
    h_margin = 0;
    uint8_t rle_optimise = (args->rle_optimise > 0) & 0x01;

    //Get palette from LIQ
    const liq_palette *liq_pal = liq_get_palette(*res);
    png_color* palette = (png_color*)malloc((liq_pal->count + rle_optimise)*sizeof(png_color));
    if (palette == NULL) {
        printf("Failed to allocate palette array for " FILENAME_FMT".\n", count);
        return;
    }

    png_byte* trans = (png_byte*)malloc(liq_pal->count + rle_optimise);
    if (trans == NULL) {
        free(palette);
        printf("Failed to allocate alpha array for " FILENAME_FMT ".\n", count);
        return;
    }

    for (k = 0; k < liq_pal->count; k++)
    {
        png_color* col = &palette[k+rle_optimise];
        //palettized as BGR, flip B and R.
        col->red = liq_pal->entries[k].b;
        col->green = liq_pal->entries[k].g;
        col->blue = liq_pal->entries[k].r;
        trans[k+rle_optimise] = liq_pal->entries[k].a;
    }

    //Get bitmap
    uint8_t* bitmap = (uint8_t*)malloc(rgba_img->width*h);
    if (bitmap == NULL) {
        free(palette);
        free(trans);
        printf("Failed to allocate bitmap array for " FILENAME_FMT ".\n", count);
        return;
    }

    liq_set_dithering_level(*res, dither_val);
    liq_write_remapped_image(*res, *img, (void*)bitmap, rgba_img->width*h);

    //One pixel of palette entry zero needs at least two bytes to be encoded with PGS.
    //This is an issue whenever the color index changes frequently due to max line coding limit.
    //To avoid RLE line length overshoot, this palette entry may not be used.
    if (rle_optimise) {
        for (k = 0; k < rgba_img->width*h; k++)
            bitmap[k] += 1;
        memcpy(&palette[0], &palette[bitmap[0]], sizeof(png_color));
        trans[0] = trans[bitmap[0]];
    }

    for (uint8_t split_cnt = 0; split_cnt < MIN(2, 1 + is_split); split_cnt++) {
        if (is_split) {
            snprintf(fname, FILENAME_MAX_LENGTH, FILENAME_FMT FILENAME_CNT FILENAME_EXT, count, split_cnt);
            w = rgba_img->crops[split_cnt].x2 - rgba_img->crops[split_cnt].x1 + 1;
            h = rgba_img->crops[split_cnt].y2 - rgba_img->crops[split_cnt].y1 + 1;
            w_margin = rgba_img->crops[split_cnt].x1;
            h_margin = rgba_img->crops[split_cnt].y1 - rgba_img->suby1;
        } else {
            w_margin = rgba_img->subx1;
            snprintf(fname, FILENAME_MAX_LENGTH, FILENAME_FMT FILENAME_EXT, count);
        }

        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        info_ptr = png_create_info_struct(png_ptr);

        if (setjmp(png_jmpbuf(png_ptr))) {
            png_destroy_write_struct(&png_ptr, &info_ptr);
            fclose(fp);
            free(trans);
            free(palette);
            free(bitmap);
            printf("Critical error in libpng while processing %s.\n", fname);
            return;
        }

        fp = fopen(fname, "wb");
        if (fp != NULL) {
            png_init_io(png_ptr, fp);
            png_set_compression_level(png_ptr, 3);

            png_set_IHDR(png_ptr, info_ptr, w, h, 8,
                         PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
                         PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

            //Write palette and alpha
            png_set_PLTE(png_ptr, info_ptr, palette, liq_pal->count + rle_optimise);
            png_set_tRNS(png_ptr, info_ptr, trans, liq_pal->count + rle_optimise, NULL);
            png_write_info(png_ptr, info_ptr);

            png_byte *row;
            for (int k = 0; k < h; k++) {
                row = (png_byte*)(bitmap + (k + h_margin)*rgba_img->width + w_margin);
                png_write_row(png_ptr, row);
            }
            png_write_end(png_ptr, NULL);
            png_destroy_write_struct(&png_ptr, &info_ptr);
            fclose(fp);
        } else {
            printf("Failed to write %s.\n", fname);
        }
    }
    free(bitmap);
    free(trans);
    free(palette);
}

static void write_png(char *fname, image_t* restrict img)
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

static void init(opts_t *args, liqopts_t *liqargs)
{
    if (fesetround(FE_TONEAREST)) {
        printf(A2B_LOG_PREFIX "failed to set configure rounding method. Bitmaps may suffer from colour drift.\n");
    }

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
    } else {
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
        liq_set_quality(attr, 0, liqargs->max_quality);
        liq_set_speed(attr, liqargs->speed);

        //Palette entry 0xFF must always be transparent.
        if (args->quantize + args->rle_optimise >= 256)
            liq_set_last_index_transparent(attr, 1);

        printf("libimagequant: Version %s\n", LIQ_VERSION_STRING);
        printf("libimagequant: Settings: max-colors=%d, max-quality=%d, speed=%d, dithering=%.02f\n",
                liq_get_max_colors(attr), liq_get_max_quality(attr), liq_get_speed(attr), liqargs->dither);
    }
}

#define _r(c)  ((c)>>24)
#define _g(c)  (((c)>>16)&0xFF)
#define _b(c)  (((c)>>8)&0xFF)
#define _a(c)  ((c)&0xFF)

#define div65025P(i) lrint(i/65025.0)
#define div255P(i) lrint(i/255.0)

#define ablend(iA, oA, iC, oC, nA) \
    lrint((iA * 255 * iC + (65025 - iA) * oC * oA) / (float)nA)

static void blend_single(image_t* restrict frame, ASS_Image *img)
{
    int x, y, c;
    uint32_t outa, k;
    uint16_t opacity = 255 - _a(img->color);
    uint8_t r = _r(img->color);
    uint8_t g = _g(img->color);
    uint8_t b = _b(img->color);

    uint8_t *src;
    uint8_t *dst;

    src = img->bitmap;
    dst = frame->buffer + img->dst_y * frame->stride + img->dst_x * 4;

    for (y = 0; y < img->h; y++) {
        for (x = 0, c = 0; x < img->w; x++, c += 4) {
            k = src[x] * opacity;

            if (k) {
                if (dst[c+3]) {
                    outa = (k * 255 + (dst[c + 3] * (65025 - k)));

                    dst[c  ] = ablend(k, dst[c+3], b, dst[c], outa);
                    dst[c+1] = ablend(k, dst[c+3], g, dst[c+1], outa);
                    dst[c+2] = ablend(k, dst[c+3], r, dst[c+2], outa);
                    dst[c+3] = div65025P(outa);
                } else {
                    dst[c  ] = b;
                    dst[c+1] = g;
                    dst[c+2] = r;
                    dst[c+3] = div255P(k);
                }
            }
        }

        src += img->stride;
        dst += frame->stride;
    }
}

#define DIM_COLOR(c, p) (uint8_t)(round(p*(float)c))

static void blend(image_t* restrict frame, ASS_Image *img, const opts_t *args)
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

                if (args->dim_flag) {
                    buf[c  ] = DIM_COLOR(buf[c  ], args->dimf);
                    buf[c+1] = DIM_COLOR(buf[c+1], args->dimf);
                    buf[c+2] = DIM_COLOR(buf[c+2], args->dimf);
                }
            }
        }

        buf += frame->stride;
    }
    if (args->full_bitmaps) {
        frame->subx1 = frame->suby1 = 0;
        frame->subx2 = frame->width - 1;
        frame->suby2 = frame->height - 1;
    }

    //Ensure minimum width and height of 8 pixels.
    c = (frame->subx2 - frame->subx1) - 8;
    if (c < 0) {
        c = abs(c);
        if (frame->subx2 + c < frame->width)
            frame->subx2 += c;
        else
            frame->subx1 -= c;
    }
    c = (frame->suby2 - frame->suby1) - 8;
    if (c < 0) {
        c = abs(c);
        if (frame->suby2 + c < frame->height)
            frame->suby2 += c;
        else
            frame->suby1 -= c;
    }
}

static void find_bbox_ysplit(image_t* restrict frame, int y_start, int y_stop, const int margin, BoundingBox_t *box)
{
    int pixelExist;
    int xk, yk;

    //left
    pixelExist = 0;
    for (xk = frame->subx1; xk <= frame->subx2 - margin && !pixelExist; xk++) {
        for (yk = y_start; yk < y_stop; yk++) {
            pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3] > 0;
        }
    }
    //sub one since inc before cmp !pixelExist
    box->x1 = MAX(xk-1, frame->subx1);

    //right
    pixelExist = 0;
    for (xk = frame->subx2; xk >= frame->subx1 + margin && !pixelExist; xk--) {
        for (yk = y_start; yk < y_stop; yk++) {
            pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3] > 0;
        }
    }
    box->x2 = MIN(xk+1, frame->subx2);

    if (y_start == frame->suby1) {
        box->y1 = frame->suby1;

        pixelExist = 0;
        for (yk = y_stop; yk >= y_start + margin && !pixelExist; yk--) {
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

static void find_bbox_xsplit(image_t* restrict frame, int x_start, int x_stop, const int margin, BoundingBox_t *box)
{
    uint8_t pixelExist;
    int xk, yk;

    //top
    pixelExist = 0;
    for (yk = frame->suby1; (yk <= frame->suby2 - margin) && !pixelExist; yk++) {
        for (xk = x_start; xk < x_stop; xk++) {
            pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3] > 0;
        }
    }
    box->y1 = MAX(yk-1, frame->suby1);

    //bottom
    pixelExist = 0;
    for (yk = frame->suby2; (yk >= frame->suby1 + margin) && !pixelExist; yk--) {
        for (xk = x_start; xk < x_stop; xk++) {
            pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3] > 0;
        }
    }
    box->y2 = MIN(yk+1, frame->suby2);

    if (x_start == frame->subx1) {
        box->x1 = frame->subx1;

        pixelExist = 0;
        for (xk = x_stop; xk >= x_start + margin && !pixelExist; xk--) {
            for (yk = box->y1; yk <= box->y2; yk++) {
                pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3] > 0;
            }
        }
        box->x2 = MIN(xk+1, x_stop);
    } else {
        box->x2 = frame->subx2;

        pixelExist = 0;
        for (xk = x_start; xk <= x_stop - margin && !pixelExist; xk++) {
            for (yk = box->y1; yk <= box->y2; yk++) {
                pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3] > 0;
            }
        }
        box->x1 = MAX(xk-1, x_start);
    }
}

static int find_split(image_t* restrict frame, opts_t *args)
{
    const int margin = 8;
    const int step = (args->split < 4) ? 8 : 1;
    uint32_t best_score = (uint32_t)(-1);
    uint8_t pixelExist;

    int yk, xk;
    memset(frame->crops, 0xFF, sizeof(BoundingBox_t)*2);

    BoundingBox_t eval[2];
    uint32_t surface = 0;

    if (frame->suby2 - frame->suby1 > margin*2) {
        //Search for a horizontal split
        for (yk = frame->suby1 + margin; yk <= frame->suby2 - margin; yk+=step) {
            pixelExist = 0;
            for (xk = frame->subx1; xk <= frame->subx2 && !pixelExist; xk++) {
                pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3];
            }

            //Line is used by data, skip to the next split.
            if (pixelExist && args->split < 4) {
                continue;
            }

            find_bbox_ysplit(frame, frame->suby1, yk, margin, &eval[0]);
            find_bbox_ysplit(frame, yk+1, frame->suby2, margin, &eval[1]);

            surface = BOX_AREA(eval[0]) + BOX_AREA(eval[1]);
            if (surface < best_score && abs(eval[0].y2 - eval[1].y1) >= args->splitmargin[0]) {
                best_score = surface;
                memcpy(frame->crops, eval, sizeof(eval));
            }
        }
    }
    if (frame->subx2 - frame->subx1 > margin*2) {
        if (args->split >= 3 || (args->split == 2 && (frame->suby2 - frame->suby1) > frame->height/2.5)) {
            //Search for a vertical split
            for (xk = frame->subx1 + margin; xk <= frame->subx2 - margin; xk+=step) {
                pixelExist = 0;
                for (yk = frame->suby1; yk <= frame->suby2 && !pixelExist; yk++) {
                    pixelExist |= frame->buffer[yk*frame->stride + xk*4 + 3];
                }

                //Line is used by data, skip to the next split.
                if (pixelExist && args->split < 4) {
                    continue;
                }

                find_bbox_xsplit(frame, frame->subx1, xk, margin, &eval[0]);
                find_bbox_xsplit(frame, xk+1, frame->subx2, margin, &eval[1]);

                surface = BOX_AREA(eval[0]) + BOX_AREA(eval[1]);
                if (surface < best_score && abs(eval[0].x2 - eval[1].x1) >= args->splitmargin[1]) {
                    best_score = surface;
                    memcpy(frame->crops, eval, sizeof(eval));
                }
            }
        }
    }
    return best_score < (uint32_t)(-1);
}

static uint64_t inline frame_to_realtime_ms(uint64_t frame_cnt, frate_t *frate)
{
    return (uint64_t)round((1000*((uint64_t)frame_cnt - 1) * frate->denom)/(double)frate->num);
}

static uint8_t diff_frames(image_t* restrict current, image_t *prev)
{
    //compare header
    if (0 != memcmp(current, prev, offsetof(image_t, out)))
        return 1;

    //header match, compare active vertical bitmap area
    return (0 != memcmp(&current->buffer[current->stride*current->suby1],
                        &prev->buffer[current->stride*current->suby1],
                        current->stride*(current->suby2 - current->suby1 + 1)));
}

static int get_frame(ASS_Renderer *renderer, ASS_Track *track, image_t* restrict prev_frame,
                     image_t* restrict frame, uint64_t frame_cnt, frate_t *frate, opts_t *args)
{
    //libass can return blank ASS_Images, we must remember whenever that happen as the changed
    //flag returned by libass becomes meaningless, and we would corrupt the event.
    static int prev_invalid = 0;
    int changed;

    uint64_t ms = frame_to_realtime_ms(frame_cnt, frate);
    ASS_Image *img = ass_render_frame(renderer, track, ms, &changed);

    if (changed && img) {
        blend(frame, img, args);

        if (frame->subx1 > -1 && frame->suby1 > -1) {
            //frame differ from the previous?
            if (NULL == prev_frame) {
                frame->in = frame_cnt;
            } else if (diff_frames(frame, prev_frame)) {
                frame->in = frame_cnt;
                memcpy(prev_frame, frame, offsetof(image_t, out));
                memcpy(prev_frame->buffer, frame->buffer, frame->stride*frame->height);
            } else {
                // img exists and is identical to prev.
                ++frame->out;
                return 1;
            }
            frame->out = frame_cnt + 1;
        } else {
            //Sometime sampling time is on an active event but the blended image is transparent
            // because the composition coefficients are weak -> discard
            prev_invalid = 1;
            if (prev_frame)
                prev_frame->in = (uint64_t)(-1);
            return 2;
        }
        prev_invalid = 0;

        return 3;
    } else if (!changed && img) {
        if (prev_invalid)
            return 2;
        ++frame->out;
        return 1;
    } else {
        //No event, change prev_frame content
        if (prev_frame)
            prev_frame->in = (uint64_t)(-1);
        prev_invalid = 0;
        return 0;
    }
}

static int quantize_event(image_t* restrict frame, liq_image **img, liq_result **qtz_res, opts_t *args)
{
    *img = liq_image_create_rgba(attr, &frame->buffer[frame->stride*frame->suby1], frame->width, frame->suby2-frame->suby1+1, 0);
    if (NULL == *img)
        return -1;

    if(liq_image_quantize(*img, attr, qtz_res) != LIQ_OK)
        return -1;

    int ret = 0;
    if (args->quantize + args->rle_optimise >= 256) {
        const liq_palette *pal = liq_get_palette(*qtz_res);
        //Using the entire palette and the last palette entry is not transparent?
        if (pal->count == args->quantize && pal->entries[pal->count - 1].a > 0) {
            const uint16_t max_colors = liq_get_max_colors(attr);

            //destroy invalid result and quantize with colors-1
            liq_result_destroy(*qtz_res);
            liq_set_max_colors(attr, max_colors - 1);

            if(liq_image_quantize(*img, attr, qtz_res) != LIQ_OK)
                ret = -1;
            //reset color count to user config
            liq_set_max_colors(attr, max_colors);
        }
    }
    return ret;
}

eventlist_t *render_subs(char *subfile, frate_t *frate, opts_t *args, liqopts_t *liqargs)
{
    long long tm = 0;
    int count = 0, fres = 0, img_cnt = 0;
    uint64_t frame_cnt = 1;
    char imgfile[FILENAME_MAX_LENGTH];

    liq_result *res;
    liq_image *img;

    eventlist_t *evlist = calloc(1, sizeof(eventlist_t));

    init(args, liqargs);
    ASS_Track *track = ass_read_file(ass_library, subfile, NULL);

    if (!track) {
        printf("track init failed!\n");
        exit(1);
    }

    printf(A2B_LOG_PREFIX "BDN format: (%dx%d), rendering at (%dx%d) for (%dx%d) display.\n", args->frame_w, args->frame_h,
           args->render_w, args->render_h, (args->par > 0 ? (int)round(args->storage_w/args->par) : args->storage_w), args->storage_h);

    image_t *frame = image_init(args->render_w, args->render_h);
    image_t *prev_frame;
    prev_frame = args->keep_dupes ? NULL : image_init(args->render_w, args->render_h);

    while (1) {
        if (fres && fres != 2 && count) {
            eventlist_set(evlist, frame, count - 1);
        }

        fres = get_frame(ass_renderer, track, prev_frame, frame, frame_cnt, frate, args);

        switch (fres) {
            case 3:
            {
                if (args->quantize && quantize_event(frame, &img, &res, args)) {
                    printf("Quantization failed for " FILENAME_FMT FILENAME_EXT ".\n", count);
                    exit(1);
                }
                if (args->split && find_split(frame, args)) {
                    if (args->quantize) {
                        write_png_palette(count, frame, &img, &res, args, 1, liqargs->dither);
                    } else {
                        for (img_cnt = 0; img_cnt < 2; img_cnt++) {
                            frame->subx1 = frame->crops[img_cnt].x1;
                            frame->subx2 = frame->crops[img_cnt].x2;
                            frame->suby1 = frame->crops[img_cnt].y1;
                            frame->suby2 = frame->crops[img_cnt].y2;
                            snprintf(imgfile, FILENAME_MAX_LENGTH, FILENAME_FMT FILENAME_CNT FILENAME_EXT, count, img_cnt);
                            write_png(imgfile, frame);
                        }
                    }
                } else {
                    if (args->quantize) {
                        write_png_palette(count, frame, &img, &res, args, 0, liqargs->dither);
                    } else {
                        snprintf(imgfile, FILENAME_MAX_LENGTH, FILENAME_FMT FILENAME_EXT, count);
                        write_png(imgfile, frame);
                    }
                }
                if (args->quantize) {
                    liq_result_destroy(res);
                    liq_image_destroy(img);
                }
                count++;
                if (args->downsampled) {
                    frame_cnt += args->downsampled;
                    frame->out += args->downsampled;
                }
            }
            /* fall through */
            case 2:
            case 1:
                ++frame_cnt;
                break;
            case 0:
            {
                tm = (uint64_t)ass_step_sub(track, frame_to_realtime_ms(frame_cnt, frate), 1);
                uint64_t offset = (tm*frate->num)/(frate->denom*1000);

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
    if (prev_frame) {
        free(prev_frame->buffer);
        free(prev_frame);
    }
    if (args->quantize && attr)
        liq_attr_destroy(attr);
    ass_free_track(track);
    ass_renderer_done(ass_renderer);
    ass_library_done(ass_library);

    return evlist;
}
