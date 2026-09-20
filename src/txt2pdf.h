/* txt2pdf.h — minimal PDF writer for plain text. Pure logic, no I/O. */
#ifndef TXT2PDF_H
#define TXT2PDF_H

#include <stddef.h>

typedef enum {
    PAGE_A4,
    PAGE_LETTER,
} tp_pagesize;

typedef struct {
    tp_pagesize size;
    int font_size;    /* points */
    double margin_pt; /* points */
    int line_numbers; /* 1 = prefix lines with numbers */
    const char *title;
} tp_options;

/* Page dimensions in points. */
void tp_page_size(tp_pagesize ps, double *w, double *h);

/* Split pre-wrapped logical lines into pages of physical lines. Returns the
 * page count. lines_per_page = floor((page_h - 2*margin) / leading) where
 * leading = font_size * 1.2. */
int tp_layout(const char *const *lines, size_t nlines, const tp_options *o,
              size_t *lines_per_page);

/* Escapes a string for a PDF literal string object: backslashes and
 * parentheses, non-ASCII -> octal escapes (WinAnsi/courier safe subset). */
size_t tp_escape_pdf_string(const char *in, char *out, size_t cap);

/* Builds the complete PDF for the given text lines into out (NUL-
 * terminated for convenience; use the returned length for the file size).
 * Returns the byte count, or -1 if out_cap is insufficient. */
long tp_build_pdf(const char *const *lines, size_t nlines,
                  const tp_options *o, char *out, size_t out_cap);

#endif
