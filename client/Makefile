# Makefile
# By Ron Bowes
# Created January, 2013
#
# (See LICENSE.txt)
#
# Should work for Linux and BSD make.

CC=gcc
COMMON_CFLAGS=-ansi -std=c89
DEBUG_CFLAGS=-g -DTESTMEMORY -Werror
CFLAGS?=-Wall ${DEBUG_CFLAGS}
LIBS=
CFLAGS+=$(COMMON_CFLAGS)

OBJS=buffer.o\
		 driver.o\
		 driver_dns.o\
		 driver_console.o\
		 tcp.o\
		 types.o\
		 memory.o\
		 dns.o\
		 log.o\
		 message.o\
		 packet.o\
		 select_group.o\
		 session.o\
		 sessions.o\
		 udp.o\
		 ui_stdin.o\
		 ui_exec.o\

DNSCAT_DNS_OBJS=${OBJS} dnscat.o
DNSCAT_TCP_OBJS=${OBJS} tcpcat.o

ENGINE_OBJS=${OBJS} engine.o

all: engine
#all: tcpcat dnscat
	@echo Compile should be complete

remove:
	rm -f /usr/local/bin/dnscat

uninstall: remove

clean:
	rm -f *.o *.exe *.stackdump dnscat tcpcat test driver_tcp driver_dns

tcpcat: ${DNSCAT_TCP_OBJS}
	-${CC} ${CFLAGS} -o tcpcat ${DNSCAT_TCP_OBJS}

dnscat: ${DNSCAT_DNS_OBJS}
	-${CC} ${CFLAGS} -o dnscat ${DNSCAT_DNS_OBJS}

engine: ${ENGINE_OBJS}
	-${CC} ${CFLAGS} -o engine ${ENGINE_OBJS}

