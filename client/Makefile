# Makefile
# By Ron Bowes
# Created January, 2013
#
# (See LICENSE.md)
#
# Should work for Linux and BSD make.

CC?=gcc
COMMON_CFLAGS=--std=c89
DEBUG_CFLAGS=-g -DTESTMEMORY -Werror -O0
RELEASE_CFLAGS=-Os
CFLAGS?=-I. -Wall -D_DEFAULT_SOURCE -fstack-protector-all -Wformat -Wformat-security ${DEBUG_CFLAGS}
LIBS=-pie -Wl,-z,relro,-z,now
CFLAGS+=$(COMMON_CFLAGS)

# TODO: This should be automatically determined
VERSION="v0.02beta"

OS=$(shell uname -s)
ARCH=$(shell if [[ `uname -p` == *"64"* ]]; then echo "x64"; else echo "x86"; fi)

ifeq ($(OS), Linux)
  RELEASE_FILENAME="dnscat2-$(VERSION)-client-$(ARCH)"
else
  RELEASE_FILENAME="dnscat2-$(VERSION)-client-$(OS)-$(ARCH)"
endif

OBJS=controller/packet.o \
		 controller/session.o \
		 controller/controller.o \
		 drivers/driver.o \
		 drivers/command/driver_command.o \
		 drivers/command/command_packet.o \
		 drivers/command/command_packet_stream.o \
		 drivers/driver_console.o \
		 drivers/driver_exec.o \
		 drivers/driver_ping.o \
		 libs/buffer.o \
		 libs/dns.o \
		 libs/log.o \
		 libs/memory.o \
		 libs/select_group.o \
		 libs/tcp.o \
		 libs/types.o \
		 libs/udp.o \
		 tunnel_drivers/driver_dns.o \

DNSCAT_DNS_OBJS=${OBJS} dnscat.o

all: dnscat
	@echo Compile should be complete

release: all
	echo ${RELEASE_FILENAME}
	@echo Release compiled

remove:
	rm -f /usr/local/bin/dnscat

uninstall: remove

clean:
	-rm -f *.o *.exe *.stackdump dnscat tcpcat test driver_tcp driver_dns
	-rm -f controller/*.o drivers/*.o libs/*.o tunnel_drivers/*.o tunnel_drivers/dns/*.o
	-rm -rf win32/Debug/
	-rm -rf win32/Release/
	-rm -rf win32/*.ncb
	-rm -rf win32/*.sln
	-rm -rf win32/*.suo
	-rm -rf win32/*.vcproj.*

dnscat: ${DNSCAT_DNS_OBJS}
	-${CC} ${CFLAGS} -o dnscat ${DNSCAT_DNS_OBJS}
