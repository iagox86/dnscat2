# Makefile
# By Ron Bowes
# Created January, 2013
#
# (See LICENSE.md)
#
# Should work for Linux and BSD make.

CC?=gcc
DEBUG_CFLAGS?=-DTESTMEMORY -Werror -O0
RELEASE_CFLAGS?=-Os
CFLAGS?=--std=c89 -I. -Wall -D_DEFAULT_SOURCE -fstack-protector-all -Wformat -Wformat-security -g
LIBS=-pie -Wl,-z,relro,-z,now

OBJS=controller/packet.o \
		 controller/session.o \
		 controller/controller.o \
		 drivers/driver.o \
		 drivers/command/driver_command.o \
		 drivers/command/command_packet.o \
		 drivers/driver_console.o \
		 drivers/driver_exec.o \
		 drivers/driver_ping.o \
		 libs/buffer.o \
		 libs/crypto/encryptor.o \
		 libs/crypto/micro-ecc/uECC.o \
		 libs/crypto/salsa20.o \
		 libs/crypto/sha3.o \
		 libs/dns.o \
		 libs/ll.o \
		 libs/log.o \
		 libs/memory.o \
		 libs/select_group.o \
		 libs/tcp.o \
		 libs/types.o \
		 libs/udp.o \
		 tunnel_drivers/driver_dns.o \

DNSCAT_DNS_OBJS=${OBJS} dnscat.o

all: dnscat
	@echo "*** Build complete! Run 'make debug' to build a debug version!"

debug: CFLAGS += $(DEBUG_CFLAGS)
debug: dnscat
	@echo "*** Debug build complete"

release: CFLAGS += ${RELEASE_CFLAGS}
release: dnscat

nocrypto: CFLAGS += -DNO_ENCRYPTION
nocrypto: all

remove:
	rm -f /usr/local/bin/dnscat

uninstall: remove

clean:
	-rm -f *.o */*.o */*/*.o */*/*/*.o *.exe *.stackdump dnscat tcpcat test driver_tcp driver_dns
	-rm -rf win32/Debug/
	-rm -rf win32/Release/
	-rm -rf win32/*.ncb
	-rm -rf win32/*.sln
	-rm -rf win32/*.suo
	-rm -rf win32/*.vcproj.*

dnscat: ${DNSCAT_DNS_OBJS}
	${CC} ${CFLAGS} -o dnscat ${DNSCAT_DNS_OBJS}
	@echo "*** dnscat successfully compiled"

COMMANDS=drivers/command/commands_standard.h \
				 drivers/command/commands_tunnel.h

drivers/command/driver_command.o: drivers/command/driver_command.c ${COMMANDS}
	${CC} -c ${CFLAGS} -o drivers/command/driver_command.o drivers/command/driver_command.c
