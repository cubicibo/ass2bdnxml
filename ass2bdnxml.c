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

#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

typedef struct frate_s {
    char *name;
    int rate;
    double frame_dur;
} frate_t;

frate_t frates[] = {
    {"23.976", 24, 1000.0 / (24000.0 / 1001.0)},
    {"24", 24, 1000.0 / (24.0 / 1.0)},
    {"25", 25, 1000.0 / (25.0 / 1.0)},
    {"29.97", 30, 1000.0 / (30000.0 / 1001.0)},
    {"50", 50, 1000.0 / (50.0 / 1.0)},
    {"59.94", 60, 1000.0 / (60000.0 / 1001.0)},
    {NULL, 0, 0}
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

static void die_usage(const char *name)
{
    printf("usage: %s <subtitle file> [options]\n", name);
    exit(1);
}

/* SMPTE non-drop time code */
void mktc(int tc, int fps, char *buf)
{
    int frames, s, m, h;
    tc++;

    frames = tc % fps;
    tc /= fps;
    s = tc % 60;
    tc /= 60;
    m = tc % 60;
    tc /= 60;
    h = tc;

    if (h > 99)
    {
        fprintf(stderr, "Timecodes above 99:59:59:99 not supported: %u:%02u:%02u:%02u\n", h, m, s, frames);
        exit(1);
    }

    if (snprintf(buf, 12, "%02d:%02d:%02d:%02d", h, m, s, frames) != 11)
    {
        fprintf(stderr, "Timecode lead to invalid format: %s\n", buf);
        exit(1);
    }
}

void write_xml(eventlist_t *evlist, vfmt_t *vfmt, frate_t *frate,
               char *track_name, char *language, opts_t *args)
{
    int x_margin = args->render_w < args->frame_w ? (args->frame_w-args->render_w) >> 1 : 0;
    int y_margin = args->render_h < args->frame_h ? (args->frame_h-args->render_h) >> 1 : 0;

    int i;
    char buf_in[12], buf_out[12];
    FILE *of = fopen("bdn.xml", "w");

    if (of == NULL) {
        perror("Error opening output XML file.");
        exit(1);
    }

    mktc(evlist->events[0]->in / frate->frame_dur, frate->rate, buf_in);
    mktc(evlist->events[evlist->nmemb - 1]->out / frate->frame_dur,
         frate->rate, buf_out);

    fprintf(of, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<BDN Version=\"0.93\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"BD-03-006-0093b BDN File Format.xsd\">\n"
                "  <Description>\n"
                "    <Name Title=\"%s\" Content=\"\"/>\n"
                "    <Language Code=\"%s\"/>\n"
                "    <Format VideoFormat=\"%s\" FrameRate=\"%s\" DropFrame=\"False\"/>\n"
                "    <Events LastEventOutTC=\"%s\" FirstEventInTC=\"%s\" ",
            track_name, language, vfmt->name, frate->name, buf_out, buf_in);

    mktc(0, frate->rate, buf_in);
    fprintf(of, "ContentInTC=\"%s\" ContentOutTC=\"%s\" NumberofEvents=\"%d\" Type=\"Graphic\"/>\n"
                "  </Description>\n"
                "  <Events>\n", buf_in, buf_out, evlist->nmemb);

    for (i = 0; i < evlist->nmemb; i++) {
        image_t *img = evlist->events[i];
        mktc(img->in / frate->frame_dur, frate->rate, buf_in);
        mktc(img->out / frate->frame_dur, frate->rate, buf_out);

        fprintf(of, "    <Event Forced=\"False\" InTC=\"%s\" OutTC=\"%s\">\n",
                buf_in, buf_out);
        fprintf(of, "      <Graphic Width=\"%d\" Height=\"%d\" X=\"%d\" Y=\"%d\">%08d.png</Graphic>\n",
                img->subx2 - img->subx1 + 1, img->suby2 - img->suby1 + 1,
                img->subx1+x_margin, img->suby1+y_margin, i);
        fprintf(of, "    </Event>\n");
    }

    fprintf(of, "  </Events>\n</BDN>\n");
    fclose(of);
}

int main(int argc, char *argv[])
{
    char *subfile = NULL;
    char *track_name = "Undefined";
    char *language = "und";
    char *video_format = "1080p";
    char *frame_rate = "23.976";
    int i;
    frate_t *frate = NULL;
    vfmt_t *vfmt = NULL;
    eventlist_t *evlist;

    opts_t args;
    memset(&args, 0, sizeof(args));

    if (argc < 2) {
        die_usage(argv[0]);
    }

    static struct option longopts[] = {
        {"dvd-mode",     no_argument,       0, 'd'},
        {"hinting",      no_argument,       0, 'g'},
        {"trackname",    required_argument, 0, 't'},
        {"language",     required_argument, 0, 'l'},
        {"video-format", required_argument, 0, 'v'},
        {"fps",          required_argument, 0, 'f'},
        {"width-render", required_argument, 0, 'w'},
        {"height-render",required_argument, 0, 'h'},
        {"width-store",  required_argument, 0, 'x'},
        {"height-store", required_argument, 0, 'y'},
        {"ass-pixratio", required_argument, 0, 'a'},
        {0, 0, 0, 0}
    };
    const char **err = NULL;

    while (1) {
        int opt_index = 0;
        int c = getopt_long(argc, argv, "dgt:l:v:f:w:h:x:y:a:", longopts, &opt_index);

        if (c == -1)
            break;

        switch (c) {
            case 'a':
                args.par = strtod(optarg, NULL);
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
            case 'd':
                args.dvd_mode = 1;
                break;
            case 'g':
                args.hinting = 1;
                break;
            case 'w':
                args.render_w = (int)strtonum(optarg, 0, 4096, err);
                if (err != NULL || args.render_w <= 0) {
                    printf("Invalid render width.");
                    exit(1);
                }
                break;
            case 'h':
                args.render_h = (int)strtonum(optarg, 0, 4096, err);
                if (err != NULL || args.render_h <= 0) {
                    printf("Invalid render height.");
                    exit(1);
                }
                break;
            case 'x':
                args.storage_w = (int)strtonum(optarg, 0, 4096, err);
                if (err != NULL || args.storage_w <= 0) {
                    printf("Invalid storage width.");
                    exit(1);
                }
                break;
            case 'y':
                args.storage_h = (int)strtonum(optarg, 0, 4096, err);
                if (err != NULL || args.storage_h <= 0) {
                    printf("Invalid storage height.");
                    exit(1);
                }
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

    // Some mapping
    if (args.render_w == 0)
        args.render_w = args.frame_w;
    if (args.render_h == 0)
        args.render_h = args.frame_h;
    if (args.render_h > args.frame_h || args.render_w > args.frame_w) {
        printf("Cannot render an ASS that has larger width or height than target.\n");
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

    args.fps = frate->rate;

    evlist = render_subs(subfile, rint(frate->frame_dur), &args);

    write_xml(evlist, vfmt, frate, track_name, language, &args);

    for (i = 0; i < evlist->nmemb; i++) {
        free(evlist->events[i]);
    }

    free(evlist);

    return 0;
}
