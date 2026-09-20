#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/txt2pdf.h"

static int checks = 0;
static int failed = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failed++; \
    fprintf(stderr, "not ok %d - %s:%d: %s\n", checks, __FILE__, __LINE__, #cond); } } while (0)

static void test_page_sizes(void) {
    double w, h;
    tp_page_size(PAGE_A4, &w, &h);
    CHECK(w > 595 && w < 596 && h > 841 && h < 842);
    tp_page_size(PAGE_LETTER, &w, &h);
    CHECK(w == 612 && h == 792);
}

static void test_layout_pages(void) {
    tp_options o = {PAGE_A4, 10, 36, 0, "t"};
    size_t lpp;
    int pages = tp_layout(NULL, 0, &o, &lpp);
    CHECK(pages >= 1);
    /* A4: 841.89 - 72 = 769.89 / 12 = 64.15 -> 64 lines per page */
    CHECK(lpp == 64);
    pages = tp_layout(NULL, 65, &o, &lpp);
    CHECK(pages == 2);
    pages = tp_layout(NULL, 128, &o, &lpp);
    CHECK(pages == 2);
    pages = tp_layout(NULL, 129, &o, &lpp);
    CHECK(pages == 3);
}

static void test_escape(void) {
    char out[256];
    tp_escape_pdf_string("hello (world) \\ back", out, sizeof out);
    CHECK(strcmp(out, "hello \\(world\\) \\\\ back") == 0);
    tp_escape_pdf_string("a\nb", out, sizeof out);
    CHECK(strcmp(out, "a\\nb") == 0);
    tp_escape_pdf_string("\xC3\xA7", out, sizeof out); /* ç in UTF-8 */
    CHECK(strncmp(out, "\\303\\247", 8) == 0);
}

static size_t count_occurrences(const char *hay, const char *needle) {
    size_t n = 0;
    const char *p = hay;
    while ((p = strstr(p, needle)) != NULL) { n++; p++; }
    return n;
}

static void test_build_pdf_structure(void) {
    const char *lines[] = {"first line", "second (line)", ""};
    tp_options o = {PAGE_A4, 10, 36, 0, "My Title"};
    char *pdf = malloc(1 << 16);
    long n = tp_build_pdf(lines, 3, &o, pdf, 1 << 16);
    CHECK(n > 500); // structurally complete for 3 short lines
    CHECK(strncmp(pdf, "%PDF-1.4", 8) == 0);
    CHECK(strstr(pdf, "/Type /Catalog") != NULL);
    CHECK(strstr(pdf, "/BaseFont /Courier") != NULL);
    CHECK(strstr(pdf, "(first line) Tj") != NULL);
    CHECK(strstr(pdf, "(second \\(line\\)) Tj") != NULL);
    CHECK(strstr(pdf, "/Title (My Title)") != NULL);
    CHECK(strstr(pdf, "startxref") != NULL);
    CHECK(strstr(pdf, "%%EOF") != NULL);
    /* one /Page object + the /Pages parent */
    CHECK(count_occurrences(pdf, "/Type /Page ") == 1);

    /* xref offsets must point at "N 0 obj" */
    const char *xref = strstr(pdf, "\nxref\n");
    CHECK(xref != NULL);
    int objcount = 0;
    sscanf(xref, "\nxref\n0 %d", &objcount);
    CHECK(objcount == 7); /* "0 7" = free row + 6 objects (1..6) */
    const char *row = xref + strlen("\nxref\n0 7\n0000000000 65535 f \n");
    for (int i = 1; i < objcount; i++) {
        char expect[32];
        snprintf(expect, sizeof expect, "%d 0 obj", i);
        long off = strtol(row, NULL, 10);
        CHECK(off > 0 && off < n);
        CHECK(strncmp(pdf + off, expect, strlen(expect)) == 0);
        row += 20; /* each xref row is exactly 20 bytes */
    }
    free(pdf);
}

static void test_multipage(void) {
    size_t lpp;
    tp_options o = {PAGE_A4, 10, 36, 0, "t"};
    tp_layout(NULL, 0, &o, &lpp);
    size_t nlines = lpp * 2 + 5;
    char **lines = malloc(nlines * sizeof *lines);
    for (size_t i = 0; i < nlines; i++) {
        lines[i] = malloc(32);
        snprintf(lines[i], 32, "line %zu", i);
    }
    char *pdf = malloc(4 << 20);
    long n = tp_build_pdf((const char *const *)lines, nlines, &o, pdf, 4 << 20);
    CHECK(n > 0);
    CHECK(count_occurrences(pdf, "/Type /Page ") == 3);
    CHECK(strstr(pdf, "(line 64) Tj") != NULL);
    CHECK(strstr(pdf, "(line 132) Tj") != NULL); // last of 133 lines
    for (size_t i = 0; i < nlines; i++) free(lines[i]);
    free(lines);
    free(pdf);
}

int main(void) {
    test_page_sizes();
    test_layout_pages();
    test_escape();
    test_build_pdf_structure();
    test_multipage();
    printf("%d checks, %d failures\n", checks, failed);
    return failed ? 1 : 0;
}
