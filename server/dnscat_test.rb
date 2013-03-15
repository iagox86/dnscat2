require 'socket'

require 'packet'

class DnscatTest
  def max_packet_size()
    return 256
  end

  def initialize()
    @data = []

    @data << {
      :send => Packet.create_syn(1234, 0, 0),
      :recv => ""
    }

    @data << {
      :send => Packet.create_syn(1234, 0, 0), # Duplicate SYN
      :recv => ""
    }

    @data << {
      :send => Packet.create_syn(4321, 0, 0),
      :recv => ""
    }

    #                            ID    SEQ  ACK  DATA
    @data << {
      :send => Packet.create_msg(1234, 0,   0,   "This is some incoming data"),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(1234, 1,   0,   "This is more data with a bad SEQ"),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(1234, 100, 0,   "This is more data with a bad SEQ"),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(1234, 26,  0,   "Data with proper SYN but bad ACK (should trigger re-send)"),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(1234, 83,  1,   ""),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(1234, 83,  1,   ""),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(1234, 83,  1,   ""),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(1234, 83,  1,   "a"),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(1234, 83,  1,   ""), # Bad SEQ
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(1234, 84,  10,  "Hello"), # Bad SEQ
      :recv => ""
    }
    @data << {
      :send => Packet.create_fin(1234),
      :recv => ""
    }

    @expected_response = nil
  end

  def recv()
    if(@data.length == 0)
      puts("Done!")
      exit
    end

    out = @data.shift
    @expected_response = out[:recv]
    return out[:send]
  end

  def send(data)
    # Just ignore the data being sent
  end
end
