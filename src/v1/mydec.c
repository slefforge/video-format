// .es -> Y4M decoder (streams to stdout)
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s [-i in.es] [-o out.y4m]\n"
                    "  If -i/-o omitted, reads stdin and writes stdout.\n", argv0);
}

static uint32_t rd32le(FILE *f) {
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static int read_header(FILE *f, uint32_t *w, uint32_t *h, uint32_t *fn, uint32_t *fd, int *chroma) {
    char magic[5] = {0};
    if (fread(magic, 1, 4, f) != 4) return 0;
    int version = fgetc(f);
    int chroma_ = fgetc(f);
    fgetc(f); fgetc(f); // reserved
    uint32_t W = rd32le(f), H = rd32le(f), FN = rd32le(f), FD = rd32le(f);

    if (strncmp(magic, "V1ES", 4) != 0) { fprintf(stderr, "bad magic\n"); return 0; }
    if (version != 1)                  { fprintf(stderr, "unsupported version\n"); return 0; }
    if (chroma_ != 1)                  { fprintf(stderr, "unsupported chroma (need 4:2:0)\n"); return 0; }
    if (!W || !H || !FN || !FD)        { fprintf(stderr, "invalid dims/fps\n"); return 0; }

    *w = W; *h = H; *fn = FN; *fd = FD; *chroma = chroma_;
    return 1;
}

// copy n bytes streaming
static int copy_n(FILE *in, FILE *out, size_t n) {
    static unsigned char buf[1 << 20]; // 1 MiB
    size_t remaining = n;
    int saw_any = 0;
    while (remaining > 0) {
        size_t chunk = remaining < sizeof buf ? remaining : sizeof buf;
        size_t r = fread(buf, 1, chunk, in);
        if (r == 0) return saw_any ? -1 : 0;
        saw_any = 1;
        if (fwrite(buf, 1, r, out) != r) return -1;
        remaining -= r;
    }
    return 1;
}

static void write_y4m_header(FILE *out, uint32_t w, uint32_t h, uint32_t fn, uint32_t fd) {
    // Progressive, no aspect info, 4:2:0
    fprintf(out, "YUV4MPEG2 W%u H%u F%u:%u Ip A0:0 C420jpeg\n", w, h, fn, fd);
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

    uint32_t w,h,fn,fd; int chroma;
    if (!read_header(in, &w, &h, &fn, &fd, &chroma)) {
        fprintf(stderr, "failed to read .es header\n"); return 1;
    }

    write_y4m_header(out, w, h, fn, fd);

    uint32_t cw = (w + 1) / 2, ch = (h + 1) / 2;
    size_t y_bytes = (size_t)w * h;
    size_t u_bytes = (size_t)cw * ch;
    size_t v_bytes = u_bytes;

    for (;;) {
        int c = fgetc(in);
        if (c == EOF) break;     // clean EOF: no extra FRAME
        ungetc(c, in);

        if (fputs("FRAME\n", out) == EOF) { perror("write"); return 1; }

        int rc = copy_n(in, out, y_bytes); if (rc < 0) { fprintf(stderr, "truncated Y\n"); return 1; }
        rc = copy_n(in, out, u_bytes);      if (rc < 0) { fprintf(stderr, "truncated U\n"); return 1; }
        rc = copy_n(in, out, v_bytes);      if (rc < 0) { fprintf(stderr, "truncated V\n"); return 1; }
    }

    if (in_path) fclose(in);
    if (out_path) fclose(out);
    return 0;
}
