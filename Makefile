# Makefile
# By Ron
#
# See LICENSE.md

all:
	@cd client && make
	@echo "Compile complete!"
	@echo "* Client: client/dnscat"
	@echo "* Server: server/dnscat_*.rb"

clean:
	@cd client && make clean
	@rm -rf dist/*


dnscat:
	@cd client && make dnscat

