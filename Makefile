# thank you sinit
# based on sinit
# sinit+ - tiny init
PREFIX   = /usr/local
CC       = cc
CPPFLAGS = -D_DEFAULT_SOURCE
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -Os
# static: init must not depend on the dynamic loader or shared libs
LDFLAGS  = -s -static

all: sinit

config.h:
	cp config.def.h $@

sinit: sinit.c config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ sinit.c $(LDFLAGS)

test:
	sh test/run.sh

clean:
	rm -f sinit

install: sinit
	mkdir -p $(DESTDIR)$(PREFIX)/sbin
	cp -f sinit $(DESTDIR)$(PREFIX)/sbin/
	chmod 755 $(DESTDIR)$(PREFIX)/sbin/sinit

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/sbin/sinit

.PHONY: all test clean install uninstall
