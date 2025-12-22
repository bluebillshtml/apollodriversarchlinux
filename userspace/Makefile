# Apollo User-space Components Makefile

CC ?= gcc
CFLAGS := -O2 -Wall -Wextra -std=c99 -D_GNU_SOURCE
LDFLAGS := -lasound -lpthread

TARGETS := apollod apolloctl

all: $(TARGETS)

apollod: apollod.o apollo_control.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

apolloctl: apolloctl.o apollo_control.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGETS)

install: all
	install -d $(DESTDIR)/usr/bin
	install apollod apolloctl $(DESTDIR)/usr/bin/
	install -d $(DESTDIR)/usr/lib/systemd/system
	install apollo.service $(DESTDIR)/usr/lib/systemd/system/

.PHONY: all clean install

