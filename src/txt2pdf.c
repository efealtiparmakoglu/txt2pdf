/* txt2pdf.c — minimal, spec-faithful PDF 1.4 writer for plain text.
 *
 * Output uses the built-in Courier font (monospaced, no embedding needed),
 * one or more pages, a correct xref table, and an Info dictionary. The
 * generated files open in every PDF reader; the layout math is pure and
 * unit-tested in tests/txt2pdf_tests.c. */
#include "txt2pdf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t floor_usable(double usable, double leading);

void tp_page_size(tp_pagesize ps, double *w, double *h) {
    if (ps == PAGE_LETTER) {
        *w = 612.0;
        *h = 792.0;
    } else { /* A4 */
        *w = 595.28;
        *h = 841.89;
    }
}

static double leading_for(const tp_options *o) {
    return o->font_size * 1.2;
}

int tp_layout(const char *const *lines, size_t nlines, const tp_options *o,
              size_t *lines_per_page) {
    double w, h;
    tp_page_size(o->size, &w, &h);
    double usable = h - 2 * o->margin_pt;
    size_t lpp = (size_t)floor_usable(usable, leading_for(o));
    *lines_per_page = lpp;
    if (nlines == 0)
        return 1; /* always at least one page */
    return (int)((nlines + lpp - 1) / lpp);
}

/* helper kept separate so the integer math is testable */
size_t floor_usable(double usable, double leading) {
    if (leading <= 0)
        return 0;
    double raw = usable / leading;
    if (raw < 1)
        return 0;
    return (size_t)raw;
}

size_t tp_escape_pdf_string(const char *in, char *out, size_t cap) {
    size_t pos = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        unsigned char c = *p;
        char tmp[8];
        int n;
        if (c == '\\' || c == '(' || c == ')') {
            n = snprintf(tmp, sizeof tmp, "\\%c", c);
        } else if (c == '\n') {
            n = snprintf(tmp, sizeof tmp, "\\n");
        } else if (c < 32 || c > 126) {
            n = snprintf(tmp, sizeof tmp, "\\%03o", c);
        } else {
            tmp[0] = (char)c;
            tmp[1] = 0;
            n = 1;
        }
        if (pos + (size_t)n + 1 > cap)
            return 0; /* insufficient */
        memcpy(out + pos, tmp, (size_t)n);
        pos += (size_t)n;
    }
    if (pos + 1 > cap)
        return 0;
    out[pos] = 0;
    return pos;
}

struct buf {
    char *data;
    size_t cap;
    size_t len;
    size_t offsets[16]; /* object offsets, up to 16 objects */
};

static void bputs(struct buf *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap)
        return;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = 0;
}

static void bprintf(struct buf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    bputs(b, tmp);
}

long tp_build_pdf(const char *const *lines, size_t nlines,
                  const tp_options *o, char *out, size_t out_cap) {
    double w, h;
    tp_page_size(o->size, &w, &h);
    size_t lpp;
    int pages = tp_layout(lines, nlines, o, &lpp);
    if (pages < 1)
        pages = 1;
    double leading = leading_for(o);
    double text_y_start = h - o->margin_pt - o->font_size;

    /* object numbering:
     * 1 catalog, 2 pages, 3 font, then per page: content (4+2k) page (5+2k)
     * last: info */
    int info_obj = 4 + pages * 2;
    int total_objs = info_obj; /* highest object number */

    if (out_cap < 4096)
        return -1;

    struct buf b = {out, out_cap, 0, {0}};
    bputs(&b, "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n");

    for (int page = 0; page < pages; page++) {
        size_t start = page * lpp;
        size_t end = start + lpp;
        if (end > nlines)
            end = nlines;
        if (nlines == 0 && page == 0)
            end = 0;

        int content_obj = 4 + page * 2;

        /* build the content stream first to know its length */
        const size_t stream_cap = 1 << 20;
        char *stream = malloc(stream_cap);
        if (!stream)
            return -1;
        size_t spos = 0;
        spos += (size_t)snprintf(stream + spos, stream_cap - spos, "BT\n/F1 %d Tf\n", o->font_size);
        spos += (size_t)snprintf(stream + spos, stream_cap - spos,
                                 "%.2f %.2f Td\n", o->margin_pt, text_y_start);
        for (size_t li = start; li < end; li++) {
            char esc[1024];
            size_t n = tp_escape_pdf_string(lines[li], esc, sizeof esc);
            if (n == 0 && lines[li][0] != 0)
                continue; /* overlong line: skip rather than corrupt */
            char prefix[32] = "";
            if (o->line_numbers)
                snprintf(prefix, sizeof prefix, "%5zu  ", li + 1);
            spos += (size_t)snprintf(stream + spos, stream_cap - spos,
                                     "(%s%s) Tj\n", prefix, esc);
            spos += (size_t)snprintf(stream + spos, stream_cap - spos, "0 -%.2f Td\n", leading);
        }
        spos += (size_t)snprintf(stream + spos, stream_cap - spos, "ET\n");
        stream[spos] = 0;

        b.offsets[content_obj - 1] = b.len;
        bprintf(&b, "%d 0 obj\n<< /Length %zu >>\nstream\n", content_obj, spos);
        bputs(&b, stream);
        bputs(&b, "endstream\nendobj\n");
        free(stream);

        int page_obj = content_obj + 1;
        b.offsets[page_obj - 1] = b.len;
        bprintf(&b, "%d 0 obj\n<< /Type /Page /Parent 2 0 R "
                     "/MediaBox [0 0 %.2f %.2f] "
                     "/Resources << /Font << /F1 3 0 R >> >> "
                     "/Contents %d 0 R >>\nendobj\n",
                page_obj, w, h, content_obj);
    }

    b.offsets[0] = b.len;
    bprintf(&b, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    b.offsets[1] = b.len;
    bprintf(&b, "2 0 obj\n<< /Type /Pages /Kids [");
    for (int page = 0; page < pages; page++)
        bprintf(&b, "%d 0 R ", 5 + page * 2);
    bprintf(&b, "] /Count %d >>\nendobj\n", pages);
    b.offsets[2] = b.len;
    bputs(&b, "3 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>\nendobj\n");

    b.offsets[info_obj - 1] = b.len;
    char esc_title[256];
    tp_escape_pdf_string(o->title ? o->title : "", esc_title, sizeof esc_title);
    bprintf(&b, "%d 0 obj\n<< /Title (%s) /Producer (txt2pdf) >>\nendobj\n",
            info_obj, esc_title);

    size_t xref_pos = b.len;
    bprintf(&b, "xref\n0 %d\n", total_objs + 1);
    bputs(&b, "0000000000 65535 f \n");
    for (int i = 0; i < total_objs; i++)
        bprintf(&b, "%010zu 00000 n \n", b.offsets[i]);
    bprintf(&b, "trailer\n<< /Size %d /Root 1 0 R /Info %d 0 R >>\n"
                 "startxref\n%zu\n%%%%EOF\n",
            total_objs + 1, info_obj, xref_pos);

    return (long)b.len;
}
