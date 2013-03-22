# Makefile
# By Ron Bowes
# Created January, 2013
#
# (See LICENSE.txt)
#
# Should work for Linux and BSD make.

CC=gcc
COMMON_CFLAGS=-ansi -std=c89
CFLAGS?=-Wall -g -DTESTMEMORY
LIBS=
CFLAGS+=$(COMMON_CFLAGS)

OBJS=dnscat.o\
		 buffer.o\
		 tcp.o\
		 types.o\
		 memory.o\
		 dns.o\
		 driver.o\
		 driver_tcp.o\
		 packet.o\
		 select_group.o\
		 session.o

all: dnscat
	@echo Compile should be complete

remove:
	rm -f /usr/local/bin/dnscat

uninstall: remove

clean:
	rm -f *.o *.exe *.stackdump dnscat test

dnscat: ${OBJS}
	${CC} ${CFLAGS} -o dnscat ${OBJS}

