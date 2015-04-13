# Makefile
# By Ron Bowes
# Created January, 2013
#
# (See LICENSE.md)
#
# Should work for Linux and BSD make.

CC?=gcc
COMMON_CFLAGS=--std=c89
#DEBUG_CFLAGS=-g -DTESTMEMORY -Werror
DEBUG_CFLAGS=-g -Werror
CFLAGS?=-Wall -D_BSD_SOURCE -O2 -fstack-protector-all -Wformat -Wformat-security ${DEBUG_CFLAGS}
LIBS=-pie -Wl,-z,relro,-z,now
CFLAGS+=$(COMMON_CFLAGS)

OBJS=buffer.o \
		 command_packet.o \
		 command_packet_stream.o \
		 driver_command.o \
		 driver_console.o \
		 driver_dns.o \
		 driver_exec.o \
		 driver_listener.o \
		 driver_ping.o \
		 tcp.o \
		 types.o \
		 memory.o \
		 dns.o \
		 log.o \
		 message.o \
		 packet.o \
		 select_group.o \
		 session.o \
		 udp.o \

DNSCAT_DNS_OBJS=${OBJS} dnscat.o
DNSCAT_TCP_OBJS=${OBJS} tcpcat.o

all: dnscat
#all: tcpcat dnscat
	@echo Compile should be complete

remove:
	rm -f /usr/local/bin/dnscat

uninstall: remove

clean:
	rm -f *.o *.exe *.stackdump dnscat tcpcat test driver_tcp driver_dns
	rm -rf win32/Debug/
	rm -rf win32/Release/
	rm -rf win32/*.ncb
	rm -rf win32/*.sln
	rm -rf win32/*.suo
	rm -rf win32/*.vcproj.*


tcpcat: ${DNSCAT_TCP_OBJS}
	-${CC} ${CFLAGS} -o tcpcat ${DNSCAT_TCP_OBJS}

dnscat: ${DNSCAT_DNS_OBJS}
	-${CC} ${CFLAGS} -o dnscat ${DNSCAT_DNS_OBJS}
