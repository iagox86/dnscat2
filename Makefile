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

OBJS=test.o buffer.o tcp.o udp.o select_group.o types.o memory.o dns.o driver_tcp.o controller.o

all: test
	@echo Compile should be complete

remove:
	rm -f /usr/local/bin/dnscat

uninstall: remove

clean:
	rm -f *.o *.exe *.stackdump dnscat

test: ${OBJS}
	${CC} ${CFLAGS} -o test ${OBJS}

