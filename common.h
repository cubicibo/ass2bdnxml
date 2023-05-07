typedef struct image_s {
    int width, height, stride, dvd_mode;
    int subx1, suby1, subx2, suby2;
    long long in, out;
    uint8_t *buffer;
} image_t;

typedef struct eventlist_s {
    int size, nmemb;
    image_t **events;
} eventlist_t;

typedef struct opts_s {
    double par;
    int fps;
    int frame_w;
    int frame_h;
    int render_w;
    int render_h;
    int dvd_mode;
    int display_w;
    int hinting;
} opts_t;

eventlist_t *render_subs(char *subfile, int frame_d, opts_t *args);
