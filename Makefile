# Makefile
# By Ron Bowes
# Created January, 2013
#
# (See LICENSE.txt)
#
# Should work for Linux and BSD make.

CC=gcc
COMMON_CFLAGS=-ansi -std=c89
CFLAGS?=-Wall -g
LIBS=
CFLAGS+=$(COMMON_CFLAGS)

all: dnscat
	@echo Compile should be complete

install: all
	mkdir -p /usr/local/bin
	cp dnscat       /usr/local/bin/dnscat
	chown root.root /usr/local/bin/dnscat

remove:
	rm -f /usr/local/bin/dnscat

uninstall: remove

clean:
	rm -f *.o *.exe *.stackdump dnscat

dnscat: dnscat.o buffer.o tcp.o udp.o select_group.o types.o memory.o dns.o
	${CC} ${CFLAGS} ${DNSCATFLAGS} -o dnscat dnscat.o buffer.o tcp.o udp.o select_group.o types.o memory.o dns.t

