require 'socket'

require 'packet'

class DnscatTest
  def max_packet_size()
    return 256
  end

  def initialize()
    @data = []

    @data << Packet.create_syn(1234, 0, 0)
    @data << Packet.create_syn(1234, 0, 0) # Duplicate SYN
    @data << Packet.create_syn(4321, 0, 0)

    #                       ID    SEQ  ACK  DATA
    @data << Packet.create_msg(1234, 0,   0,   "This is some incoming data")
    @data << Packet.create_msg(1234, 1,   0,   "This is more data with a bad SEQ")
    @data << Packet.create_msg(1234, 100, 0,   "This is more data with a bad SEQ")
    @data << Packet.create_msg(1234, 26,  0,   "Data with proper SYN but bad ACK (should trigger re-send)")
    @data << Packet.create_msg(1234, 83,  1,   "")
    @data << Packet.create_msg(1234, 83,  1,   "")
    @data << Packet.create_msg(1234, 83,  1,   "")
    @data << Packet.create_msg(1234, 83,  1,   "a")
    @data << Packet.create_msg(1234, 83,  1,   "") # Bad SEQ
    @data << Packet.create_msg(1234, 84,  10,  "Hello") # Bad SEQ
    @data << Packet.create_fin(1234)
  end

  def recv()
    if(@data.length == 0)
      puts("Done!")
      exit
    end

    return @data.shift()
  end

  def send(data)
    # Just ignore the data being sent
  end
end
