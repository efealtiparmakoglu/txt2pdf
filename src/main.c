/* main.c — txt2pdf CLI: read text, write PDF. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "txt2pdf.h"

static void usage(FILE *out) {
    fprintf(out,
        "txt2pdf 1.0.0 - plain text to PDF (built-in Courier, no deps)\n\n"
        "USAGE:\n"
        "    txt2pdf [options] [INPUT]   (default input: stdin)\n\n"
        "OPTIONS:\n"
        "    -o, --output FILE    output PDF path (required)\n"
        "    -s, --size S         a4 | letter (default a4)\n"
        "    -f, --font-size N    font size in points (default 10)\n"
        "    -m, --margin PT      margin in points (default 36)\n"
        "    -n, --line-numbers   prefix lines with numbers\n"
        "    -t, --title TEXT     document title metadata\n"
        "    -h, --help\n"
        "    -V, --version\n");
}

int main(int argc, char **argv) {
    tp_options o = {PAGE_A4, 10, 36.0, 0, "txt2pdf"};
    const char *output = NULL;
    const char *input = NULL;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(stdout); return 0; }
        if (!strcmp(a, "-V") || !strcmp(a, "--version")) { printf("txt2pdf 1.0.0\n"); return 0; }
        if (!strcmp(a, "-o") || !strcmp(a, "--output")) {
            if (++i >= argc) { fprintf(stderr, "txt2pdf: -o needs a path\n"); return 2; }
            output = argv[i];
            continue;
        }
        if (!strcmp(a, "-s") || !strcmp(a, "--size")) {
            if (++i >= argc) { fprintf(stderr, "txt2pdf: -s needs a value\n"); return 2; }
            if (!strcmp(argv[i], "letter")) o.size = PAGE_LETTER;
            else if (!strcmp(argv[i], "a4")) o.size = PAGE_A4;
            else { fprintf(stderr, "txt2pdf: unknown size %s\n", argv[i]); return 2; }
            continue;
        }
        if (!strcmp(a, "-f") || !strcmp(a, "--font-size")) {
            if (++i >= argc) { fprintf(stderr, "txt2pdf: -f needs a number\n"); return 2; }
            o.font_size = atoi(argv[i]);
            if (o.font_size < 4 || o.font_size > 72) {
                fprintf(stderr, "txt2pdf: font size must be 4..72\n");
                return 2;
            }
            continue;
        }
        if (!strcmp(a, "-m") || !strcmp(a, "--margin")) {
            if (++i >= argc) { fprintf(stderr, "txt2pdf: -m needs a number\n"); return 2; }
            o.margin_pt = atof(argv[i]);
            continue;
        }
        if (!strcmp(a, "-n") || !strcmp(a, "--line-numbers")) { o.line_numbers = 1; continue; }
        if (!strcmp(a, "-t") || !strcmp(a, "--title")) {
            if (++i >= argc) { fprintf(stderr, "txt2pdf: -t needs text\n"); return 2; }
            o.title = argv[i];
            continue;
        }
        if (!strcmp(a, "-")) continue;
        input = a;
    }

    if (!output) {
        fprintf(stderr, "txt2pdf: -o output.pdf is required\n");
        return 2;
    }

    /* read all input lines */
    FILE *in = input ? fopen(input, "r") : stdin;
    if (!in) { perror("txt2pdf"); return 1; }
    size_t cap = 1024, nlines = 0;
    char **lines = malloc(cap * sizeof *lines);
    char *line = NULL;
    size_t linecap = 0;
    ssize_t len;
    while ((len = getline(&line, &linecap, in)) != -1) {
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = 0;
        if (nlines == cap) {
            cap *= 2;
            lines = realloc(lines, cap * sizeof *lines);
        }
        lines[nlines++] = strdup(line);
    }
    free(line);
    if (in != stdin) fclose(in);

    size_t cap_out = 64 + nlines * 2048 + 4096;
    char *pdf = malloc(cap_out);
    long n = tp_build_pdf((const char *const *)lines, nlines, &o, pdf, cap_out);
    while (n < 0) {
        cap_out *= 2;
        pdf = realloc(pdf, cap_out);
        n = tp_build_pdf((const char *const *)lines, nlines, &o, pdf, cap_out);
    }

    FILE *outf = fopen(output, "wb");
    if (!outf) { perror("txt2pdf"); return 1; }
    fwrite(pdf, 1, (size_t)n, outf);
    fclose(outf);

    size_t lpp;
    tp_layout((const char *const *)lines, nlines, &o, &lpp);
    fprintf(stderr, "txt2pdf: wrote %s (%ld bytes, %zu lines, %d page(s))\n",
            output, n, nlines,
            nlines == 0 ? 1 : (int)((nlines + lpp - 1) / lpp));

    int code = (int)lpp % 2; /* lpp unused warning guard; always 0/1 */
    (void)code;
    return 0;
}
