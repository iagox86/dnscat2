# Makefile
# By Ron
#
# See LICENSE.md

# Can't use a '#' in the shell command
VERSION=$(shell egrep '^.define VERSION' client/dnscat.c | head -n1 | cut -d\" -f2)

OS=$(shell uname -s)
ARCH=$(shell uname -p | sed 's/x86_64/x64/i' | sed 's/i.86/x86/i')

ifeq ($(OS), Linux)
  RELEASE_FILENAME="dnscat2-$(VERSION)-client-$(ARCH)"
else
  RELEASE_FILENAME="dnscat2-$(VERSION)-client-$(OS)-$(ARCH)"
endif

all:
	@cd client && make
	@echo "Compile complete!"
	@echo "* Client: client/dnscat"
	@echo "* Server: server/dnscat_*.rb"

clean:
	@cd client && make clean
	@rm -rf dist/*

debug:
	@cd client && make debug
	@echo "Debug compile complete!"

release: clean
	-mkdir dist/
	@cd client && make release
	@mv client/dnscat .
	@strip dnscat
	@tar -cvvjf dist/${RELEASE_FILENAME}.tar.bz2 dnscat
	@echo "*** Release compiled: `pwd`/${RELEASE_FILENAME}"
	@echo "*** By the way, did you update the version number in the server?"
	@echo "Release compile complete!"

source_release: clean
	-mkdir dist/
	@cp -r client dnscat2_client
	@tar -cvvjf dist/dnscat2-${VERSION}-client-source.tar.bz2 dnscat2_client
	@zip -r dist/dnscat2-${VERSION}-client-source.zip dnscat2_client
	@rm -rf dnscat2_client
	@cp -r server dnscat2_server
	@tar -cvvjf dist/dnscat2-${VERSION}-server.tar.bz2 dnscat2_server
	@zip -r dist/dnscat2-${VERSION}-server.zip dnscat2_server
	@rm -rf dnscat2_server

dnscat:
	@cd client && make dnscat

