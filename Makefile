all:
	@cd client && make
	@echo "Compile complete!"
	@echo "* Client: client/dnscat"
	@echo "* Server: server/dnscat_*.rb"

clean:
	@cd client && make clean

dnscat:
	@cd client && make dnscat

