##
# driver_tcp.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
# A TCP wrapper for the dnscat2 protocol (mostly for testing)
##

require 'socket'

class DriverTCP
  def initialize(s)
    @s = s
  end

  def recv()
    loop do
      length = @s.read(2)
      if(length.nil? || length.length != 2)
        raise(IOError, "Connection closed while reading the length")
      end
      length = length.unpack("n").shift

      incoming = @s.read(length)
      if(incoming.nil? || incoming.length != length)
        raise(IOError, "Connection closed while reading packet")
      end

      outgoing = yield(incoming, 32767)
      if(!outgoing.nil?)
        outgoing = [outgoing.length, outgoing].pack("nA*")
        @s.write(outgoing)
      end
    end
  end

  def close()
    @s.close
  end

  def DriverTCP.go(host, port)
    Log.WARNING(nil, "Starting Dnscat2 TCP server on #{host}:#{port}...")
    server = TCPServer.new(port)

    loop do
      Thread.start(server.accept) do |s|
        Log.INFO(nil, "Received a new connection from #{s.peeraddr[3]}:#{s.peeraddr[1]} (#{s.peeraddr[2]})...")

        begin
          tcp = DriverTCP.new(s)
          Dnscat2.go(tcp)
        rescue IOError => e
          puts("IOError caught: #{e.inspect}")
          puts(e.inspect)
          puts(e.backtrace)
        end
      end
    end
  end
end
