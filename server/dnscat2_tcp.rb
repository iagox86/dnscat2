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

  def send(data)
    data = [data.length, data].pack("nA*")
    @s.write(data)
  end

  def recv()
    length = @s.read(2)
    if(length.nil? || length.length != 2)
      raise(IOError, "Connection closed while reading the length")
    end
    length = length.unpack("n").shift

    data = @s.read(length)
    if(data.nil? || data.length != length)
      raise(IOError, "Connection closed while reading packet")
    end

    return data
  end

  def close()
    @s.close
  end
end

# TODO: Remove this
#require 'session'
#Session.debug_set_isn(0x4444)
# TODO: Remove this

port = 2000

server = TCPServer.new(port)
Log.log("TCP", "Listening for connections on #{server.addr[3]}:#{server.addr[1]} (#{server.addr[2]})...")

loop do
  s = server.accept
  #Thread.start(server.accept) do |s|
    Log.log("TCP", "Received a new connection from #{s.peeraddr[3]}:#{s.peeraddr[1]} (#{s.peeraddr[2]})...")

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
