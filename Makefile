CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L
PREFIX  ?= /usr/local
DESTDIR ?=

all: txt2pdf

txt2pdf: src/main.c src/txt2pdf.c src/txt2pdf.h
	$(CC) $(CFLAGS) -o $@ src/main.c src/txt2pdf.c -lm

pdf_tests: tests/txt2pdf_tests.c src/txt2pdf.c src/txt2pdf.h
	$(CC) $(CFLAGS) -o pdf_tests tests/txt2pdf_tests.c src/txt2pdf.c

test: txt2pdf pdf_tests
	./pdf_tests
	printf 'hello pdf\nsecond line\n' | ./txt2pdf -o /tmp/t2p-smoke.pdf && \
	strings /tmp/t2p-smoke.pdf | grep -q "(hello pdf) Tj"

install: txt2pdf
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -m 755 txt2pdf "$(DESTDIR)$(PREFIX)/bin/txt2pdf"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/txt2pdf"

clean:
	rm -f txt2pdf pdf_tests

.PHONY: all test install uninstall clean
