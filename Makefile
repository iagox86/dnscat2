# Makefile
# By Ron
#
# See LICENSE.md

all:
	@cd client && make
	@echo "Compile complete!"
	@echo "* Client: client/dnscat"
	@echo "* Server: server/dnscat_*.rb"

debug:
	@cd client && make debug
	@echo "Debug compile complete!"

release:
	@cd client && make release
	@echo "Release compile complete!"

clean:
	@cd client && make clean
	@rm -rf dist/*

dnscat:
	@cd client && make dnscat

