#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s [-i in.y4m] [-o out.es]\n"
                    "  If -i/-o omitted, reads stdin and writes stdout.\n", argv0);
}

void parse_y4m_header(char *line, uint32_t *w, uint32_t *h, uint32_t *fn, uint32_t *fd, int *c420) {
    char *token = strtok(line, " ");
    while (token != NULL) {
        if (strncmp(token, "W", 1) == 0) {
            *w = (uint32_t)atoi(token + 1);
        } else if (strncmp(token, "H", 1) == 0) {
            *h = (uint32_t)atoi(token + 1);
        } else if (strncmp(token, "F", 1) == 0) {
            char *colon = strchr(token + 1, ':');
            if (colon) {
                *colon = '\0';
                *fn = (uint32_t)atoi(token + 1);
                *fd = (uint32_t)atoi(colon + 1);
            }
        } else if (strncmp(token, "C420", 4) == 0) {
            *c420 = 1; // accept C420, C420jpeg, C420paldv, etc.
        }
        token = strtok(NULL, " ");
    }
}

static void write_u32_le(FILE *f, uint32_t v) {
    uint8_t b[4] = { (uint8_t)(v), (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    if (fwrite(b, 1, 4, f) != 4) { perror("write"); exit(1); }
}

static void write_es_header(FILE *out, uint32_t w, uint32_t h,
                            uint32_t fps_num, uint32_t fps_den, int c420) {
    if (fwrite("V1ES", 1, 4, out) != 4) { perror("write"); exit(1); }
    if (fputc(1, out) == EOF) { perror("write"); exit(1); }                 // version
    if (fputc(c420 ? 1 : 0, out) == EOF) { perror("write"); exit(1); }      // chroma
    if (fputc(0, out) == EOF || fputc(0, out) == EOF) { perror("write"); exit(1); } // reserved
    write_u32_le(out, w); write_u32_le(out, h); write_u32_le(out, fps_num); write_u32_le(out, fps_den);
}

// Validate that a frame has the proper "FRAME" header line
static int validate_frame_header(FILE *in) {
    char hdr[2048];
    if (!fgets(hdr, sizeof hdr, in)) return 0;     // clean EOF
    if (strncmp(hdr, "FRAME", 5) != 0) return -1; // bad
    return 1;
}

// Stream copy exactly n bytes; return 1 on success, 0 on short read/write
static int copy_exact(FILE *in, FILE *out, size_t n) {
    static uint8_t buf[1 << 20]; // 1 MiB scratch
    while (n > 0) {
        size_t chunk = n < sizeof buf ? n : sizeof buf;
        size_t r = fread(buf, 1, chunk, in);
        if (r == 0) return 0;
        size_t w = fwrite(buf, 1, r, out);
        if (w != r) return 0;
        n -= r;
        if (r < chunk) {
            if (n != 0) return 0;
        }
    }
    return 1;
}

static int copy_frame_420(FILE *in, FILE *out, uint32_t w, uint32_t h) {
    uint32_t cw = (w + 1) / 2, ch = (h + 1) / 2;
    size_t y_bytes = (size_t)w * h;
    size_t u_bytes = (size_t)cw * ch;
    size_t v_bytes = u_bytes;
    if (!copy_exact(in, out, y_bytes)) return 0;
    if (!copy_exact(in, out, u_bytes)) return 0;
    if (!copy_exact(in, out, v_bytes)) return 0;
    return 1;
}

int main(int argc, char **argv) {
    const char *in_path = NULL, *out_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-i") && i + 1 < argc) in_path = argv[++i];
        else if (!strcmp(argv[i], "-o") && i + 1 < argc) out_path = argv[++i];
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
    }

    FILE *in = in_path ? fopen(in_path, "rb") : stdin;
    if (!in) { perror("open input"); return 1; }
    FILE *out = out_path ? fopen(out_path, "wb") : stdout;
    if (!out) { perror("open output"); return 1; }

    // 2. Parse Y4M header
    char line[1024];
    if (!fgets(line, sizeof line, in)) { fprintf(stderr, "failed to read Y4M header\n"); return 1; }
    if (strncmp(line, "YUV4MPEG2", 9) != 0) { fprintf(stderr, "not a Y4M stream\n"); return 1; }

    uint32_t w = 0, h = 0, fn = 25, fd = 1;
    int c420 = 0;
    parse_y4m_header(line, &w, &h, &fn, &fd, &c420);
    if (!(w && h && fd && c420)) { fprintf(stderr, "unsupported Y4M header (need C420)\n"); return 1; }

    // 3. Write ES header
    write_es_header(out, w, h, fn, fd, c420);

    // 4. Loop frames
    size_t frames = 0;
    for (;;) {
        int eh = validate_frame_header(in);
        if (eh == 0) { fprintf(stderr, "Done. Wrote %zu frames.\n", frames); break; }
        if (eh < 0)  { fprintf(stderr, "Bad frame header. Aborting.\n"); return 1; }
        if (!copy_frame_420(in, out, w, h)) {
            fprintf(stderr, "Truncated frame payload at frame %zu. Aborting.\n", frames);
            return 1;
        }
        frames++;
    }
    if (in_path) fclose(in);
    if (out_path) fclose(out);
    return 0;
}
