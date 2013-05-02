##
# dnscat2_tcp.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# A TCP wrapper for the dnscat2 protocol (mostly for testing)
##

$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'dnscat2_server'
require 'log'
require 'socket'

class DnscatTCP
  def max_packet_size()
    return 1024
  end

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

      outgoing = yield(incoming)
      if(!outgoing.nil?)
        outgoing = [outgoing.length, outgoing].pack("nA*")
        @s.write(outgoing)
      end
    end
  end

  def close()
    @s.close
  end
end

Log.set_min_level(Log::LOG_INFO)

port = 2000

server = TCPServer.new(port)
Log.INFO("Listening for connections on #{server.addr[3]}:#{server.addr[1]} (#{server.addr[2]})...")

loop do
  s = server.accept
  #Thread.start(server.accept) do |s|
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

      exit
    end
  #end
end
