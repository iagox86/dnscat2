# Makefile
# By Ron Bowes
# Created January, 2013
#
# (See LICENSE.md)
#
# Should work for Linux and BSD make.

CC?=gcc
COMMON_CFLAGS=-ansi -std=c89
#DEBUG_CFLAGS=-g -DTESTMEMORY -Werror
DEBUG_CFLAGS=-g -Werror
INCLUDES?=-I controller/ -I drivers/ -I libs/ -I tunnel_drivers/ -I tunnel_drivers/dns/ 
CFLAGS?=$(INCLUDES) -Wall -D_BSD_SOURCE -O2 -fstack-protector-all -Wformat -Wformat-security ${DEBUG_CFLAGS}
LIBS=-pie -Wl,-z,relro,-z,now
CFLAGS+=$(COMMON_CFLAGS)

OBJS=controller/command_packet.o \
		 controller/command_packet_stream.o \
		 controller/packet.o \
		 controller/session.o \
		 drivers/driver_command.o \
		 drivers/driver_console.o \
		 drivers/driver_exec.o \
		 drivers/driver_listener.o \
		 drivers/driver_ping.o \
		 libs/buffer.o \
		 libs/log.o \
		 libs/memory.o \
		 libs/message.o \
		 libs/select_group.o \
		 libs/tcp.o \
		 libs/types.o \
		 libs/udp.o \
		 tunnel_drivers/dns/dns.o \
		 tunnel_drivers/dns/driver_dns.o \

DNSCAT_DNS_OBJS=${OBJS} dnscat.o

all: dnscat
	@echo Compile should be complete

remove:
	rm -f /usr/local/bin/dnscat

uninstall: remove

clean:
	rm -f *.o *.exe *.stackdump dnscat tcpcat test driver_tcp driver_dns
	rm -f controller/*.o drivers/*.o libs/*.o tunnel_drivers/*.o tunnel_drivers/dns/*.o
	rm -rf win32/Debug/
	rm -rf win32/Release/
	rm -rf win32/*.ncb
	rm -rf win32/*.sln
	rm -rf win32/*.suo
	rm -rf win32/*.vcproj.*

dnscat: ${DNSCAT_DNS_OBJS}
	-${CC} ${CFLAGS} -o dnscat ${DNSCAT_DNS_OBJS}
