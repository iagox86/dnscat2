##
# dnscat2_test.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# Self tests for dnscat2_server.rb - implements a fake "client" that's
# basically just a class.
#
# NOTE: Run this by using 'ruby dnscat2_server.rb --test'
##

$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'packet'
require 'log'
require 'session_manager'

class Test
  MY_DATA = "this is MY_DATA"
  MY_DATA2 = "this is MY_DATA2"
  MY_DATA3 = "this is MY_DATA3"
  THEIR_DATA = "This is THEIR_DATA"

  THEIR_ISN = 0x4444
  MY_ISN    = 0x5555

  SESSION_ID = 0x1234
  KILLED_SESSION_ID = 0x4321

  OVERFLOW_SESSION_ID = 0x1111
  OVERFLOW_MY_ISN     = 0xFFFE
  OVERFLOW_THEIR_ISN  = 0xFFFD

  MAX_LENGTH = 0xFF

  def initialize()
    @data = []

    my_seq     = MY_ISN
    their_seq  = THEIR_ISN
    packet_id  = rand(0xFFFF)

    @data << {
      :send => Packet.create_msg(packet_id, KILLED_SESSION_ID, my_seq, their_seq, MY_DATA),
      :recv => Packet.create_fin(packet_id, KILLED_SESSION_ID),
      :name => "Sending an unexpected MSG (should respond with a FIN)",
    }

    @data << {
      :send => Packet.create_fin(packet_id, SESSION_ID),
      :recv => Packet.create_fin(packet_id, SESSION_ID),
      :name => "Sending an unexpected FIN (should respond with a FIN)",
    }

    @data << {
      :send => Packet.create_syn(packet_id, SESSION_ID, my_seq),
      :recv => Packet.create_syn(packet_id, SESSION_ID, their_seq),
      :name => "Initial SYN (SEQ 0x%04x => 0x%04x)" % [my_seq, their_seq],
    }

    @data << {
      :send => Packet.create_syn(packet_id, SESSION_ID, MY_ISN, 0), # Duplicate SYN
      :recv => nil,
      :name => "Duplicate SYN (should be ignored)",
    }

    @data << {
      :send => Packet.create_syn(packet_id, 0x4321, MY_ISN),
      :recv => Packet.create_syn(packet_id, 0x4321, THEIR_ISN),
      :name => "Initial SYN, session 0x4321 (SEQ 0x5555 => 0x4444) (should create new session)",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,    their_seq,               MY_DATA),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq + MY_DATA.length, THEIR_DATA),
      :name => "Sending some initial data",
    }
    my_seq += MY_DATA.length # Update my seq

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq+1,   0,     "This is more data with a bad SEQ"),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq, THEIR_DATA),
      :name => "Sending data with a bad SEQ (too low), this should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq - 100,   0,   "This is more data with a bad SEQ"),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq, THEIR_DATA),
      :name => "Sending data with a bad SEQ (way too low), this should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq+100, 0,   "This is more data with a bad SEQ"),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq, THEIR_DATA),
      :name => "Sending data with a bad SEQ (too high), this should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,    their_seq,                 MY_DATA2),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq + MY_DATA2.length,  THEIR_DATA),
      :name => "Sending another valid packet, but with a bad ACK, causing the server to repeat the last message",
    }
    my_seq += MY_DATA2.length

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,    their_seq ^ 0xFFFF, ""),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq,             THEIR_DATA),
      :name => "Sending a packet with a very bad ACK, which should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,    their_seq - 1,      ""),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq,             THEIR_DATA),
      :name => "Sending a packet with a slightly bad ACK (one too low), which should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,    their_seq + THEIR_DATA.length + 1, ""),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq,                            THEIR_DATA),
      :name => "Sending a packet with a slightly bad ACK (one too high), which should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,        their_seq + 1, ""),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq + 1, my_seq,        THEIR_DATA[1..-1]),
      :name => "ACKing the first byte of their data, which should cause them to send the second byte and onwards",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,        their_seq + 1, ""),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq + 1, my_seq,        THEIR_DATA[1..-1]),
      :name => "ACKing just the first byte again",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,        their_seq + 1,             MY_DATA3),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq + 1, my_seq + MY_DATA3.length,  THEIR_DATA[1..-1]),
      :name => "Still ACKing the first byte, but sending some more of our own data",
    }
    my_seq += MY_DATA3.length

    their_seq += THEIR_DATA.length
    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,    their_seq, ''),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq,    ''),
      :name => "ACKing their data properly, they should respond with nothing",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,    their_seq, ''),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq,    ''),
      :name => "Sending a blank MSG packet, expecting to receive a black MSG packet",
    }

    @data << {
      :send => Packet.create_syn(packet_id, SESSION_ID, my_seq),
      :recv => nil,
      :name => "Attempting to send a SYN before the FIN - should be ignored",
    }

    @data << {
      :send => Packet.create_fin(packet_id, SESSION_ID),
      :recv => Packet.create_fin(packet_id, SESSION_ID),
      :name => "Sending a FIN, should receive a FIN",
    }

    # Re-set the ISNs
    my_seq     = MY_ISN - 1000
    their_seq  = THEIR_ISN
    @data << {
      :send => Packet.create_syn(packet_id, SESSION_ID, my_seq),
      :recv => Packet.create_syn(packet_id, SESSION_ID, their_seq),
      :name => "Attempting re-use the old session id - this should work flawlessly",
    }

    @data << {
      :send => Packet.create_msg(packet_id, SESSION_ID, my_seq,    their_seq,               MY_DATA),
      :recv => Packet.create_msg(packet_id, SESSION_ID, their_seq, my_seq + MY_DATA.length, ""),
      :name => "Sending initial data in the new session",
    }
    my_seq += MY_DATA.length # Update my seq

    # Re-set the ISNs
    my_seq     = MY_ISN - 1000
    their_seq  = THEIR_ISN
    @data << {
      :send => Packet.create_syn(packet_id, 0x4411, my_seq),
      :recv => Packet.create_syn(packet_id, 0x4411, their_seq),
      :name => "Attempting re-use the old session id - this should work flawlessly",
    }

    @data << {
      :send => Packet.create_msg(packet_id, 0x4411, my_seq,    their_seq,               MY_DATA),
      :recv => Packet.create_msg(packet_id, 0x4411, their_seq, my_seq + MY_DATA.length, ""),
      :name => "Sending initial data in the new session",
    }

    # Close both sessions
    @data << {
      :send => Packet.create_fin(packet_id, SESSION_ID),
      :recv => Packet.create_fin(packet_id, SESSION_ID),
      :name => "Sending a FIN, should receive a FIN",
    }

    @data << {
      :send => Packet.create_fin(packet_id, 0x4411),
      :recv => Packet.create_fin(packet_id, 0x4411),
      :name => "Sending a FIN, should receive a FIN",
    }

    @data << {
      :send => Packet.create_fin(packet_id, SESSION_ID),
      :recv => Packet.create_fin(packet_id, SESSION_ID),
      :name => "Sending a FIN for a session that's already closed, it should ignore it",
    }

    my_seq = OVERFLOW_MY_ISN
    their_seq = OVERFLOW_THEIR_ISN
    @data << {
      :send => Packet.create_syn(packet_id, OVERFLOW_SESSION_ID, my_seq),
      :recv => Packet.create_syn(packet_id, OVERFLOW_SESSION_ID, their_seq),
      :name => "Sending a SYN for a session that will quickly overflow",
    }

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq,    their_seq,               MY_DATA),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq, my_seq + MY_DATA.length, THEIR_DATA),
      :name => "Sending data that will cause an overflow of the sequence number",
    }

    my_seq += MY_DATA.length # Update my seq

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq+1,   0,     "This is more data with a bad SEQ"),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq, my_seq, THEIR_DATA),
      :name => "Sending data with a bad SEQ (too low), this should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq - 1,   0,   "This is more data with a bad SEQ"),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq, my_seq, THEIR_DATA),
      :name => "Sending data with a bad SEQ (way too low), this should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq+100, 0,   "This is more data with a bad SEQ"),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq, my_seq, THEIR_DATA),
      :name => "Sending data with a bad SEQ (too high), this should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq,    their_seq,                 MY_DATA2),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq, my_seq + MY_DATA2.length,  THEIR_DATA),
      :name => "Sending another valid packet, with data, but with a bad ACK, causing the server to repeat the last message",
    }
    my_seq += MY_DATA2.length

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq,    their_seq ^ 0x1234, ""),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq, my_seq,             THEIR_DATA),
      :name => "Sending a packet with a very bad ACK, which should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq,    their_seq - 1,      ""),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq, my_seq,             THEIR_DATA),
      :name => "Sending a packet with a slightly bad ACK (one too low), which should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq,    their_seq + THEIR_DATA.length + 1, ""),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq, my_seq,                            THEIR_DATA),
      :name => "Sending a packet with a slightly bad ACK (one too high), which should trigger a re-send",
    }

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq,        their_seq + 1, ""),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq + 1, my_seq,        THEIR_DATA[1..-1]),
      :name => "ACKing the first byte of their data, which should cause them to send the second byte and onwards",
    }

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq,        their_seq + 1, ""),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq + 1, my_seq,        THEIR_DATA[1..-1]),
      :name => "ACKing just the first byte again",
    }

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq,        their_seq + 1,             MY_DATA3),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq + 1, my_seq + MY_DATA3.length,  THEIR_DATA[1..-1]),
      :name => "Still ACKing the first byte, but sending some more of our own data",
    }
    my_seq += MY_DATA3.length

    their_seq += THEIR_DATA.length
    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq,    their_seq, ''),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq, my_seq,    ''),
      :name => "ACKing their data properly, they should respond with nothing",
    }

    @data << {
      :send => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, my_seq,    their_seq, ''),
      :recv => Packet.create_msg(packet_id, OVERFLOW_SESSION_ID, their_seq, my_seq,    ''),
      :name => "Sending a blank MSG packet, expecting to receive a black MSG packet",
    }


    return
  end

  def recv()
    loop do
      if(@data.length == 0)
        raise(IOError, "Connection closed")
      end

      out = @data.shift
      response = yield(out[:send], MAX_LENGTH)

      if(response != out[:recv])
        @@failure += 1
        puts(out[:name])
        puts(" >> Expected: #{out[:recv].nil? ? "<no response> " : Packet.parse(out[:recv])}")
        if(response.nil?)
          puts(" >> Received: [nil]")
        else
          puts(" >> Received: #{Packet.parse(response)}")
        end
      else
        @@success += 1
        puts("SUCCESS: #{out[:name]}")
      end
    end
  end

  def send(data)
    # Just ignore the data being sent
  end

  def close()
    # Do nothing
  end

  def Test.do_test()
    begin
      @@success = 0
      @@failure = 0

      Session.debug_set_isn(THEIR_ISN)

      # Create a 'good' session
      s = SessionManager.create_session(SESSION_ID)
      s.queue_outgoing(THEIR_DATA)

      # Create a session that we'll kill right away
      s = SessionManager.create_session(KILLED_SESSION_ID)

      # Create a session whose SEQ will overflow right away
      s = SessionManager.create_session(OVERFLOW_SESSION_ID)
      s.debug_set_seq(OVERFLOW_THEIR_ISN)
      s.queue_outgoing(THEIR_DATA)

      # Do the tests
      SessionManager.go(Test.new)
    rescue IOError => e
      puts("IOError was thrown (as expected): #{e}")
      puts("Tests passed: #{@@success} / #{@@success + @@failure}")
    end

    exit
  end

  def Test.log(level, message)
    puts("#{Log::LEVELS[level]} :: #{message}")
  end
end

Log.subscribe(Test)
Test.do_test()

