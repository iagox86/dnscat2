$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'socket'

require 'dnscat2_server'
require 'log'
require 'packet'

class DnscatTest
  SESSION_QUEUE = "This is some outgoing data queued up!"

  def max_packet_size()
    return 256
  end

  def initialize()
    @data = []

    session_id = 0x1234
    my_seq     = 0x3333
    their_seq  = 0x4444

    @data << {
    #                            ID      ISN
      :send => Packet.create_syn(session_id, my_seq),
      :recv => Packet.create_syn(session_id, their_seq),
      :name => "Initial SYN (SEQ 0x%04x => 0x%04x)" % [my_seq, their_seq],
    }

    #                            ID      ISN     Options
    @data << {
      :send => Packet.create_syn(session_id, 0x3333, 0), # Duplicate SYN
      :recv => nil,
      :name => "Duplicate SYN (should be ignored)",
    }

    #                            ID      ISN
    @data << {
      :send => Packet.create_syn(0x4321, 0x5555),
      :recv => Packet.create_syn(0x4321, 0x4444),
      :name => "Initial SYN, session 0x4321 (SEQ 0x5555 => 0x4444) (should create new session)",
    }

    return # TODO: Enable more tests as we figure things out

    #                            ID          SEQ     ACK          DATA
    @data << {
      :send => Packet.create_msg(session_id, my_seq, their_seq,   "This is some incoming data"),
      :recv => ""
    }

    @data << {
      :send => Packet.create_msg(session_id, 1,   0,   "This is more data with a bad SEQ"),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(session_id, 100, 0,   "This is more data with a bad SEQ"),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(session_id, 26,  0,   "Data with proper SYN but bad ACK (should trigger re-send)"),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(session_id, 83,  1,   ""),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(session_id, 83,  1,   ""),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(session_id, 83,  1,   ""),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(session_id, 83,  1,   "a"),
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(session_id, 83,  1,   ""), # Bad SEQ
      :recv => ""
    }
    @data << {
      :send => Packet.create_msg(session_id, 84,  10,  "Hello"), # Bad SEQ
      :recv => ""
    }
    @data << {
      :send => Packet.create_fin(session_id),
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
    @current_test = out[:name]

    return out[:send]
  end

  def send(data)
    if(data != @expected_response)
      Log.log("FAIL", @current_test)
      puts(" >> Expected: #{@expected_response.unpack("H*")}")
      puts(" >> Received: #{data.unpack("H*")}")
    else
      Log.log("SUCCESS", @current_test)
    end
    # Just ignore the data being sent
  end

  def DnscatTest.do_test()
    Session.debug_set_isn(0x4444)
    session = Session.find(0x1234)
    session.queue_outgoing(SESSION_QUEUE)
    Dnscat2.go(DnscatTest.new)
  end
end

DnscatTest.do_test()
