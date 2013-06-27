##
# dnscat2_tcp.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# A TCP wrapper for the dnscat2 protocol (mostly for testing)
##

require 'log'
require 'socket'

class DnscatTCP
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

  def DnscatTCP.go(host, port)
    Log.WARNING "Starting Dnscat2 TCP server on #{host}:#{port}..."
    server = TCPServer.new(port)

    loop do
      Thread.start(server.accept) do |s|
        Log.INFO("Received a new connection from #{s.peeraddr[3]}:#{s.peeraddr[1]} (#{s.peeraddr[2]})...")

        begin
          tcp = DnscatTCP.new(s)
          Dnscat2.go(tcp)
        rescue IOError => e
          puts("IOError caught: #{e.inspect}")
          puts(e.inspect)
          puts(e.backtrace)
        rescue Exception => e
          puts("Exception caught: #{e.inspect}")
          puts(e.inspect)
          puts(e.backtrace)
        end
      end
    end
  end
end
