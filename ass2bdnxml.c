/*
 * Copyright © 2015, Martin Herkt <lachs0r@srsfckn.biz>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or  without fee is hereby granted,  provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING  FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Additional changes: Copyright © 2023, cubicibo
 * The same agreement notice applies.
 */

#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

frate_t frates[] = {
    {"23.976",24, 24000, 1001},
    {"24", 24, 24, 1},
    {"25", 25, 25, 1},
    {"29.97", 30, 30000, 1001},
    {"50", 50, 50, 1},
    {"59.94", 60, 60000, 1001},
    {NULL, 0, 0, 0}
};

typedef struct vfmt_s {
    char *name;
    int w;
    int h;
} vfmt_t;

vfmt_t vfmts[] = {
    {"1080p", 1920, 1080},
    {"1080i", 1920, 1080},
    {"720p", 1280, 720},
    {"576i", 720, 576},
    {"480p", 720, 480},
    {"480i", 720, 480},
    {NULL, 0, 0}
};


#define OPT_ARGS_DIFF   999

#define OPT_LIQ_SPEED   1000
#define OPT_LIQ_DITHER  1001
#define OPT_LIQ_MAXQUAL 1002

static void die_usage(const char *name)
{
    printf("usage: %s <subtitle file> [options]\n", name);
    exit(1);
}

static void tc_to_tcarray(char *buf, uint8_t *vals)
{
    uint8_t hits = 0;

    for (uint8_t k = 0; k < 12; k++) {
        if(buf[k] && buf[k] >= '0' && buf[k] <= '9' && (k % 3 != 2)) {
            //Do nothing, getting integer as expected
        } else if (k == 11 || (buf[k] && buf[k] == ':' && (k % 3 == 2) && hits < 4)) {
            vals[hits] = 10*(buf[k-2] - '0') + (buf[k-1] - '0');
            hits++;
        } else {
            fprintf(stderr, "NDF TC format for offset parameter is invalid: %s\n", buf);
            exit(1);
        }
    }
    if (hits < 4) {
        fprintf(stderr, "Failed to parse TC offset (too few ':'?): %s.\n", buf);
        exit(1);
    }
}

static uint64_t tcarray_to_frame(uint8_t *vals, frate_t *fps)
{
    if (vals[3] >= fps->rate) {
        printf("Frame in TC is above framerate: %d >= %d\n", vals[3], fps->rate);
        exit(1);
    }
    uint64_t offset = (uint64_t)vals[3];
    offset += fps->rate*(uint64_t)vals[2];
    offset += 60*fps->rate*(uint64_t)vals[1];
    offset += 3600*fps->rate*(uint64_t)vals[0];
    return offset;
}

static void parse_margins(char *buf, uint16_t *margins) 
{
    uint8_t k = 0;
    uint8_t idx = 0;
    for (k = 0; k < 9 && buf[k]; k++) {
        if (buf[k] == 'x' && idx == 0) {
            idx = 1;
        } else if (buf[k] >= '0' && buf[k] <= '9') {
            margins[idx] = margins[idx]*10 + (uint16_t)(buf[k] - '0');
        } else /*if ((buf[k] == 'x' && idx == 1) || (idx == 0 && k > 4))*/ {
            printf("Invalid margins format, expected 'VxH' (akin to 1080x1920), got %s\n", buf);
            exit(1);
        }
    }
    if (k >= 5 && idx == 0) {
        printf("Invalid margins format.\n");
        exit(1);
    }
}

static void frame_to_tc(uint64_t frames, frate_t *fps, char *buf)
{
    frames--;
    uint8_t  frame = frames % fps->rate;
    uint64_t ts = frames/fps->rate;
    uint8_t  sec = ts % 60;
    ts /= 60;
    uint8_t m = ts % 60;
    ts /= 60;
    if (ts > 99) {
        fprintf(stderr, "timestamp overflow (more than 99 hours).\n");
        exit(1);
    } else if (snprintf(buf, 12, "%02d:%02d:%02d:%02d", (uint8_t)ts, m, sec, frame) != 11) {
        fprintf(stderr, "Timecode lead to invalid format: %s\n", buf);
        exit(1);
    }
}

void write_xml(eventlist_t *evlist, vfmt_t *vfmt, frate_t *frate, char *bdnfile,
               char *track_name, char *language, opts_t *args)
{
    int x_margin = args->render_w < args->frame_w ? (args->frame_w-args->render_w) >> 1 : 0;
    int y_margin = args->render_h < args->frame_h ? (args->frame_h-args->render_h) >> 1 : 0;

    int i;
    char buf_in[12], buf_out[12];
    FILE *of;
    if (bdnfile == NULL) {
        of = fopen("bdn.xml", "w");
    } else {
        of = fopen(bdnfile, "w");
    }

    if (of == NULL) {
        perror("Error opening output XML file.");
        exit(1);
    }

    frame_to_tc(evlist->events[0]->in + args->offset, frate, buf_in);
    frame_to_tc(evlist->events[evlist->nmemb - 1]->out + args->offset, frate, buf_out);

    fprintf(of, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<BDN Version=\"0.93\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"BD-03-006-0093b BDN File Format.xsd\">\n"
                "  <Description>\n"
                "    <Name Title=\"%s\" Content=\"\"/>\n"
                "    <Language Code=\"%s\"/>\n"
                "    <Format VideoFormat=\"%s\" FrameRate=\"%s\" DropFrame=\"False\"/>\n"
                "    <Events LastEventOutTC=\"%s\" FirstEventInTC=\"%s\" ",
            track_name, language, vfmt->name, frate->name, buf_out, buf_in);

    //No idea what this ContentInTC truly means.
    if (args->offset > 0)
        frame_to_tc(1 + args->offset, frate, buf_in);

    fprintf(of, "ContentInTC=\"%s\" ContentOutTC=\"%s\" NumberofEvents=\"%d\" Type=\"Graphic\"/>\n"
                "  </Description>\n"
                "  <Events>\n", buf_in, buf_out, evlist->nmemb);

    for (i = 0; i < evlist->nmemb; i++) {
        image_t *img = evlist->events[i];
        frame_to_tc(img->in + args->offset, frate, buf_in);
        frame_to_tc(img->out + args->offset, frate, buf_out);

        fprintf(of, "    <Event Forced=\"False\" InTC=\"%s\" OutTC=\"%s\">\n",
                buf_in, buf_out);
        if (img->crops[0].x1 & 0xFF000000) {
            fprintf(of, "      <Graphic Width=\"%d\" Height=\"%d\" X=\"%d\" Y=\"%d\">%08d.png</Graphic>\n",
                    img->subx2 - img->subx1 + 1, img->suby2 - img->suby1 + 1,
                    img->subx1+x_margin, img->suby1+y_margin, i);
        } else {
            for (uint8_t ki = 0; ki < 2; ki++) {
                fprintf(of, "      <Graphic Width=\"%d\" Height=\"%d\" X=\"%d\" Y=\"%d\">%08d_%d.png</Graphic>\n",
                    img->crops[ki].x2 - img->crops[ki].x1 + 1, img->crops[ki].y2 - img->crops[ki].y1 + 1,
                    img->crops[ki].x1+x_margin, img->crops[ki].y1+y_margin, i, ki);
            }
        }
        fprintf(of, "    </Event>\n");
    }

    fprintf(of, "  </Events>\n</BDN>\n");
    fclose(of);
}

int main(int argc, char *argv[])
{
    char *subfile = NULL;
    char *bdnfile = NULL;
    char *track_name = "Undefined";
    char *language = "und";
    char *video_format = "1080p";
    char *frame_rate = "23.976";
    int i;
    frate_t *frate = NULL;
    vfmt_t *vfmt = NULL;
    eventlist_t *evlist;

    uint8_t liq_params = 0;
    uint8_t copy_name = 0;
    uint8_t negative_offset = 0;
    uint8_t offset_vals[4];
    memset(offset_vals, 0, sizeof(offset_vals));

    opts_t args;
    liqopts_t liqargs = {.dither=1.0f, .speed=4, .max_quality=100};

    memset(&args, 0, sizeof(args));

    if (argc < 2) {
        die_usage(argv[0]);
    }

    static struct option longopts[] = {
        {"dvd-mode",     no_argument,       0, 'd'},
        {"hinting",      no_argument,       0, 'g'},
        {"negative",     no_argument,       0, 'z'},
        {"rleopt",       no_argument,       0, 'r'},
        {"copyname",     no_argument,       0, 'c'},
        {"split",        required_argument, 0, 's'},
        {"splitmargin",  required_argument, 0, 'm'},
        {"trackname",    required_argument, 0, 't'},
        {"language",     required_argument, 0, 'l'},
        {"video-format", required_argument, 0, 'v'},
        {"fps",          required_argument, 0, 'f'},
        {"width-render", required_argument, 0, 'w'},
        {"height-render",required_argument, 0, 'h'},
        {"width-store",  required_argument, 0, 'x'},
        {"height-store", required_argument, 0, 'y'},
        {"par",          required_argument, 0, 'p'},
        {"fontdir",      required_argument, 0, 'a'},
        {"offset",       required_argument, 0, 'o'},
        {"quantize",     required_argument, 0, 'q'},
        {"no-dupes",     no_argument,       0, OPT_ARGS_DIFF},
        {"liq-dither",   required_argument, 0, OPT_LIQ_DITHER},
        {"liq-quality",  required_argument, 0, OPT_LIQ_MAXQUAL},
        {"liq-speed",    required_argument, 0, OPT_LIQ_SPEED},
        {0, 0, 0, 0}
    };

    while (1) {
        int opt_index = 0;
        int c = getopt_long(argc, argv, "czdgjrt:l:v:f:w:h:x:y:p:a:o:q:s:m:k:", longopts, &opt_index);

        if (c == -1)
            break;

        switch (c) {
            case 'a':
                args.fontdir = optarg;
                break;
            case 'c':
                copy_name = 1;
                break;
            case 'd':
                args.dvd_mode = 1;
                break;
            case 'z':
                negative_offset = 1;
                break;
            case 'r':
                args.rle_optimise = 1;
                break;
            case 'g':
                args.hinting = 1;
                break;
            case 't':
                track_name = optarg;
                break;
            case 'l':
                language = optarg;
                break;
            case 'v':
                video_format = optarg;
                break;
            case 'f':
                frame_rate = optarg;
                break;
            case 'p':
                args.par = strtod(optarg, NULL);
                if (args.par < 0.1 || args.par > 10) {
                    printf("Unusual PAR, aborting.\n");
                    exit(1);
                }
                args.par = 1./args.par;
                break;
            case 's':
                args.split = (uint8_t)strtol(optarg, NULL, 10);
                if (args.split > 3) {
                    printf("Invalid split mode.\n");
                    exit(1);
                }
                break;
            case 'm':
                parse_margins(optarg, args.splitmargin);
                break;
            case 'o':
                tc_to_tcarray(optarg, offset_vals);
                break;
            case 'w':
                args.render_w = (int)strtol(optarg, NULL, 10);
                if (args.render_w <= 0 || args.render_w > 4096) {
                    printf("Invalid render width.\n");
                    exit(1);
                }
                break;
            case 'h':
                args.render_h = (int)strtol(optarg, NULL, 10);
                if (args.render_h <= 0 || args.render_h > 4096) {
                    printf("Invalid render height.\n");
                    exit(1);
                }
                break;
            case 'x':
                args.storage_w = (int)strtol(optarg, NULL, 10);
                if (args.storage_w <= 0 || args.storage_w  > 4096) {
                    printf("Invalid storage width.\n");
                    exit(1);
                }
                break;
            case 'y':
                args.storage_h = (int)strtol(optarg, NULL, 10);
                if (args.storage_h <= 0 || args.storage_h > 4096) {
                    printf("Invalid storage height.\n");
                    exit(1);
                }
                break;
            case 'q':
                args.quantize = (uint16_t)strtol(optarg, NULL, 10);
                if (args.quantize > 255) {
                    printf("Colours must be within [0; 255] incl. (default: 0, no quantization, output are 32-bit RGBA PNGs).\n");
                    exit(1);
                }
                args.quantize += (args.quantize == 1);
                break;
            case OPT_ARGS_DIFF:
                args.find_dupes = 1;
                break;
            case OPT_LIQ_SPEED:
                liqargs.speed = (uint8_t)strtol(optarg, NULL, 10);
                if (liqargs.speed == 0 || liqargs.speed > 10) {
                    printf("Invalid libimagequant speed setting. Must be within [1; 10] incl. Default: 4.\n");
                    exit(1);
                }
                liq_params |= 1;
                break;
            case OPT_LIQ_MAXQUAL:
                liqargs.max_quality = (uint8_t)strtol(optarg, NULL, 10);
                if (liqargs.max_quality == 0 || liqargs.max_quality > 100) {
                    printf("Invalid libimagequant max quality setting. Must be within [1; 100] incl. Default: 100.\n");
                    exit(1);
                }
                liq_params |= 1;
                break;
            case OPT_LIQ_DITHER:
                liqargs.dither = (float)strtod(optarg, NULL);
                if (liqargs.dither > 1.0f || liqargs.dither < 0.0f) {
                    printf("Dithering level must be within [0.0; 1.0] incl. Default: 1 (enabled, maximum).\n");
                    exit(1);
                }
                liq_params |= 1;
                break;
            default:
                die_usage(argv[0]);
                break;
        }
    }

    if (argc - optind == 1)
        subfile = argv[optind];
    else {
        printf("Only a single input file allowed.\n");
        exit(1);
    }
    if (copy_name) {
        int len = strlen(subfile);
        int ext_pos = -1;

        for (i = len-1; i >= -1; --i)
        {
            if (i == -1 || subfile[i] == '\\'|| subfile[i] == '/') {
                bdnfile = (char*)malloc(len - i + 1);
                memcpy(bdnfile, &subfile[i+1], len - i);
                break;
            } else if (subfile[i] == '.' && ext_pos == -1 && i < len-3) {
                ext_pos = i;
            }
        }
        if (bdnfile == NULL || ext_pos == -1) {
            printf("Failed to identify filename or not standard ASS extension.\n");
            exit(1);
        }
        bdnfile[len - i - 4] = 'x';
        bdnfile[len - i - 3] = 'm';
        bdnfile[len - i - 2] = 'l';
        bdnfile[len - i - 1] = 0;
    }

    i = 0;

    while (frates[i].name != NULL)
    {
        if (!strcasecmp(frates[i].name, frame_rate))
            frate = &frates[i];

        i++;
    }

    if (frate == NULL) {
        printf("Invalid framerate.\n");
        exit(1);
    }

    i = 0;

    while (vfmts[i].name != NULL)
    {
        if (!strcasecmp(vfmts[i].name, video_format))
            vfmt = &vfmts[i];

        i++;
    }

    if (vfmt == NULL) {
        printf("Invalid video format.\n");
        exit(1);
    }
    args.frame_h = vfmt->h;
    args.frame_w = vfmt->w;

    if ((args.splitmargin[1] > (args.frame_h*3)/4) || (args.splitmargin[0] > (args.frame_w*3)/4)) {
        printf("Excessive split margin(s), should be less than 3/4 of video height or width.\n");
        exit(1);
    }

    // Some mapping
    if (args.render_w == 0)
        args.render_w = args.frame_w;
    if (args.render_h == 0)
        args.render_h = args.frame_h;
    if (args.render_h > args.frame_h || args.render_w > args.frame_w) {
        printf("Cannot render ASS file with larger width or height than video format.\n");
        exit(1);
    }
    if (args.storage_h > 0 || args.storage_w > 0) {
        if (args.storage_h == 0) {
            args.storage_h = args.render_h;
        }
        if (args.storage_w == 0) {
            args.storage_w = args.render_w;
        }
    }
    //Compute timing offset
    args.offset = tcarray_to_frame(offset_vals, frate);
    if (negative_offset)
        args.offset *= -1;

    if (args.quantize) {
        //RLE optimise discard palette entry zero, we have 254 usable colors.
        if (args.rle_optimise && args.quantize == 255)
            args.quantize -= 1;
        liqargs.max_quality = MAX(0, MIN(100, liqargs.max_quality));
    } else if (liq_params) {
        printf("Set up libimagequant parameters but not using --quantize.\n");
        exit(1);
    }

    evlist = render_subs(subfile, frate, &args, &liqargs);

    write_xml(evlist, vfmt, frate, bdnfile, track_name, language, &args);

    for (i = 0; i < evlist->nmemb; i++) {
        free(evlist->events[i]);
    }

    free(evlist);
    if (bdnfile)
        free(bdnfile);

    return 0;
}
