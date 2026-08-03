CC = clang
CFLAGS = -O2 -Wall -Wextra
LDFLAGS ?=

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man/man1

kd: kd.c kf.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ kd.c

install: kd
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)
	install -m 755 kd $(DESTDIR)$(BINDIR)/kd
	install -m 644 kd.1 $(DESTDIR)$(MANDIR)/kd.1

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/kd $(DESTDIR)$(MANDIR)/kd.1

clean:
	rm -f kd

.PHONY: install uninstall clean
