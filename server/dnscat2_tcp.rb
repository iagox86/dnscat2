require 'dnscat2'
require 'socket'

class DnscatTCP
  def init(port)
    @s = nil
  end

  def send(data)
    data = [data.length, data].pack("nA*")
    @s.send(data)
  end

  def recv(data)
    length = @s.read(2)
    if(length.length != 2)
      raise(IOError, "Connection closed while reading the length")
    end
  end

  def close()

  end
end

server = TCPServer.new(2000)

loop do
  Thread.start(server.accept) do |s|
    Dnscat2.go(DnscatTCP.new(s))
  end
end
