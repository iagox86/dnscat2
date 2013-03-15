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
  # TODO
  end
end
