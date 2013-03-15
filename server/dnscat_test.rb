$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'socket'

require 'dnscat2_server'
require 'log'
require 'packet'

class DnscatTest
  MY_DATA = "This is some incoming data"
  MY_DATA2 = "This is some more incoming data"
  MY_DATA3 = "abcdef"
  THEIR_DATA = "This is some outgoing data queued up!"

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

    @data << {
    #                            ID      ISN     Options
      :send => Packet.create_syn(session_id, 0x3333, 0), # Duplicate SYN
      :recv => nil,
      :name => "Duplicate SYN (should be ignored)",
    }

    @data << {
    #                            ID      ISN
      :send => Packet.create_syn(0x4321, 0x5555),
      :recv => Packet.create_syn(0x4321, 0x4444),
      :name => "Initial SYN, session 0x4321 (SEQ 0x5555 => 0x4444) (should create new session)",
    }

    @data << {
    #                            ID          SEQ        ACK                      DATA
      :send => Packet.create_msg(session_id, my_seq,    their_seq,               MY_DATA),
      :recv => Packet.create_msg(session_id, their_seq, my_seq + MY_DATA.length, THEIR_DATA),
      :name => "Sending some initial data",
    }
    my_seq += MY_DATA.length # Update my seq

    @data << {
    #                            ID          SEQ         ACK  DATA
      :send => Packet.create_msg(session_id, my_seq+1,   0,   "This is more data with a bad SEQ"),
      :recv => nil,
      :name => "Sending data with a bad SEQ (too low), this should be ignored",
    }

    @data << {
    #                            ID          SEQ             ACK  DATA
      :send => Packet.create_msg(session_id, my_seq - 100,   0,   "This is more data with a bad SEQ"),
      :recv => nil,
      :name => "Sending data with a bad SEQ (way too low), this should be ignored",
    }

    @data << {
    #                            ID          SEQ         ACK  DATA
      :send => Packet.create_msg(session_id, my_seq+100, 0,   "This is more data with a bad SEQ"),
      :recv => nil,
      :name => "Sending data with a bad SEQ (too high), this should be ignored",
    }

    @data << {
    #                            ID          SEQ        ACK                        DATA
      :send => Packet.create_msg(session_id, my_seq,    their_seq,                 MY_DATA2),
      :recv => Packet.create_msg(session_id, their_seq, my_seq + MY_DATA2.length,  THEIR_DATA),
      :name => "Sending another valid packet, but with a bad ACK, causing the server to repeat the last message",
    }
    my_seq += MY_DATA2.length

    @data << {
      :send => Packet.create_msg(session_id, my_seq,        their_seq + 1, ""),
      :recv => Packet.create_msg(session_id, their_seq + 1, my_seq,        THEIR_DATA[1..-1]),
      :name => "ACKing the first byte of their data, which should cause them to send the second byte and onwards",
    }

    @data << {
      :send => Packet.create_msg(session_id, my_seq,        their_seq + 1, ""),
      :recv => Packet.create_msg(session_id, their_seq + 1, my_seq,        THEIR_DATA[1..-1]),
      :name => "ACKing just the first byte again",
    }

    @data << {
      :send => Packet.create_msg(session_id, my_seq,        their_seq + 1,             MY_DATA3),
      :recv => Packet.create_msg(session_id, their_seq + 1, my_seq + MY_DATA3.length,  THEIR_DATA[1..-1]),
      :name => "Still ACKing the first byte, but sending some more of our own data",
    }
    my_seq += MY_DATA3.length

    their_seq += THEIR_DATA.length
    @data << {
      :send => Packet.create_msg(session_id, my_seq,    their_seq, ''),
      :recv => Packet.create_msg(session_id, their_seq, my_seq,    ''),
      :name => "ACKing their data properly, they should respond with nothing",
    }

    @data << {
      :send => Packet.create_msg(session_id, my_seq,    their_seq, ''),
      :recv => Packet.create_msg(session_id, their_seq, my_seq,    ''),
      :name => "Sending a blank MSG packet, expecting to receive a black MSG packet",
    }

    return # TODO: Enable more tests as we figure things out

    @data << {
      :send => Packet.create_fin(session_id),
      :recv => Packet.create_fin(session_id),
      :name => "Sending a FIN, should receive a FIN",
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
      puts(" >> Expected: #{Packet.parse(@expected_response)}")
      puts(" >> Received: #{Packet.parse(data)}")
    else
      Log.log("SUCCESS", @current_test)
    end
    # Just ignore the data being sent
  end

  def DnscatTest.do_test()
    Session.debug_set_isn(0x4444)
    session = Session.find(0x1234)
    session.queue_outgoing(THEIR_DATA)
    Dnscat2.go(DnscatTest.new)
  end
end

DnscatTest.do_test()
