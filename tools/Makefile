# Apollo Driver Development Tools Makefile

CC ?= gcc
CFLAGS := -O2 -Wall -Wextra -std=c99 -D_GNU_SOURCE
LDFLAGS :=

TARGETS := apollo_detect apollo_dump apollo_test apollo_activate

all: $(TARGETS)

apollo_detect: apollo_detect.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

apollo_dump: apollo_dump.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

apollo_test: apollo_test.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

apollo_activate: apollo_activate
	@echo "Apollo activate is a script, no compilation needed"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGETS)

install: all
	install -d $(DESTDIR)/usr/bin
	install apollo_detect apollo_dump apollo_test $(DESTDIR)/usr/bin/
	install -m 755 apollo_activate $(DESTDIR)/usr/bin/

uninstall:
	rm -f $(DESTDIR)/usr/bin/apollo_detect
	rm -f $(DESTDIR)/usr/bin/apollo_dump
	rm -f $(DESTDIR)/usr/bin/apollo_test

.PHONY: all clean install uninstall
