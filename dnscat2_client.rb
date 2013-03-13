require 'socket'

# This is for testing only

# Message types
MESSAGE_TYPE_SYN        = 0x00
MESSAGE_TYPE_MSG        = 0x01
MESSAGE_TYPE_FIN        = 0x02
MESSAGE_TYPE_STRAIGHTUP = 0xFF

# Session states [TODO: Do I actually need these?]
SESSION_STATE_NEW      = 0x00
SESSION_STATE_ACTIVE   = 0x01
SESSION_STATE_FINISHED = 0x02

def send(s, data)
  data = [data.length].pack("n") + data
  puts(data.unpack("H*"))
  s.send(data)
end

s = TCPSocket.open("localhost", 2000)
send(s, [MESSAGE_TYPE_SYN, 0x1234].pack("Cn"))

#a = s.read()
#s.close()
