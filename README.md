# txt2pdf

**Plain text to PDF in pure C — built-in Courier, hand-built xref table, zero dependencies beyond libc.**

[![CI](https://github.com/efealtiparmakoglu/txt2pdf/actions/workflows/ci.yml/badge.svg)](https://github.com/efealtiparmakoglu/txt2pdf/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C11](https://img.shields.io/badge/standard-C11-blue.svg)]()

```
$ txt2pdf -o notes.pdf -t "Meeting Notes" notes.txt
txt2pdf: wrote notes.pdf (2418 bytes, 47 lines, 1 page(s))
```

`txt2pdf` turns plain text into valid PDF 1.4 files. The writer emits every
structure itself — catalog, page tree, content streams, Info dictionary and
a byte-exact cross-reference table — using the built-in Courier font so no
font embedding is needed. The format never changes under your feet: the PDF
1.4 spec is frozen.

## Why txt2pdf

- **Hand-built xref, byte-exact.** The cross-reference offsets point exactly
  at each object; the unit tests verify this structurally.
- **Monospaced layout, no metrics.** Courier's fixed advance width means
  line breaking is pure integer math — simple, predictable, testable.
- **Real escaping.** Parentheses, backslashes and non-ASCII bytes are
  octal-escaped per the PDF string syntax.
- **Multipage.** Lines flow across pages automatically; `--line-numbers`
  marks original positions.
- **Zero dependencies.** libc only.

## Install

```sh
make            # builds ./txt2pdf
make install    # PREFIX=/usr/local by default
```

## Usage

```sh
txt2pdf -o out.pdf input.txt
txt2pdf -o out.pdf --size letter -n --title "Report" input.txt
cat error.log | txt2pdf -o errors.pdf -f 8
```

| Option | Meaning |
|---|---|
| `-o, --output FILE` | output PDF path (required) |
| `-s, --size a4\|letter` | page size (default a4) |
| `-f, --font-size N` | font size in points (default 10) |
| `-m, --margin PT` | margin in points (default 36) |
| `-n, --line-numbers` | number the original lines |
| `-t, --title TEXT` | document title metadata |

## Development

```sh
make test    # unit tests + CLI smoke
```

Tests verify page-size tables, layout pagination math, PDF string escaping,
the structural completeness of generated files (header, objects, xref
offsets pointing at real objects, EOF marker), and multipage line flow.

## License

MIT — see [LICENSE](LICENSE).
