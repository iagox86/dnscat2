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

OBJS=dnscat.o      \
		 buffer.o      \
		 tcp.o         \
		 types.o       \
		 memory.o      \
		 dns.o         \
		 packet.o      \
		 select_group.o\
		 session.o     \
		 udp.o         \

DNSCAT_DNS_OBJS=driver_dns.o
DNSCAT_TCP_OBJS=driver_tcp.o

all: dnscat tcpcat
	@echo Compile should be complete

remove:
	rm -f /usr/local/bin/dnscat

uninstall: remove

clean:
	rm -f *.o *.exe *.stackdump dnscat tcpcat test

driver_tcp.o: ${OBJS} driver_tcp.c
	${CC} ${CFLAGS} -c -o driver_tcp.o driver_tcp.c

driver_dns.o: ${OBJS} driver_dns.c
	${CC} ${CFLAGS} -c -o driver_dns.o driver_dns.c

tcpcat: ${OBJS} driver_tcp.o
	-${CC} ${CFLAGS} -o tcpcat ${OBJS} driver_tcp.o

dnscat: ${OBJS} driver_dns.o
	-${CC} ${CFLAGS} -o dnscat ${OBJS} driver_dns.o

