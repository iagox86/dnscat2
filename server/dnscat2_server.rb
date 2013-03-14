$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'socket'

require 'session'
require 'packet'

# This class should be totally stateless, and rely on the Session class
# for any long-term session storage
class Dnscat2
  def Dnscat2.handle_syn(packet, session)
    if(!session.syn_valid?())
      raise(IOError, "SYN invalid in this state")
    end

    session.set_their_seq(packet.seq)
    session.set_established()

    return Packet.create_syn(session.id, session.my_seq, nil)
  end

  def Dnscat2.handle_msg(packet, session)
    if(!session.msg_valid?())
      raise(IOError, "MSG invalid in this state")
    end

    # Validate the sequence number
    if(packet.seq != session.their_seq)
      puts("Bad sequence number; expected #{session.seq}, got #{packet.seq}")
      return
    end

    # Acknowledge the data that has been received so far
    session.ack_outgoing(packet.ack)

    # Write the incoming data to the session
    session.queue_incoming(packet.data)

    # Increment the expected sequence number
    session.increment_their_seq(packet.data.length)

    # Get any data we have queued
    data = session.read_outgoing()

    # Build the new packet
    return Packet.create_msg(session.id,
                             session.my_seq,
                             session.their_seq,
                             data)
  end

  def Dnscat2.go(s)
    session_id = nil
    begin
      loop do
        packet = Packet.read(s)
        session = Session.find(packet.session_id)

        puts(packet.inspect)

        response = nil
        if(packet.type == Packet::MESSAGE_TYPE_SYN)
          response = handle_syn(packet, session)
        elsif(packet.type == Packet::MESSAGE_TYPE_MSG)
          response = handle_msg(packet, session)
        elsif(packet.type == Packet::MESSAGE_TYPE_FIN)
          response = handle_fin(packet, session)
        else
          raise(IOError, "Unknown packet type: #{packet.type}")
        end

        if(response)
          Packet.write(s, response)
        end
      end
    rescue IOError => e
      if(!session_id.nil?)
        # TODO Send a FIN if we can
        Session.destroy(session_id)
      end

      puts(e.inspect)
      puts(e.backtrace)
    end

    s.close()
  end
end

class Test
  def initialize()
    @data = ''
  end

  def queue(data)
    @data += [data.length].pack("n")
    @data += data
  end

  def read(n)
    if(@data.length < n)
      raise(Exception, "No data left")
    end

    response = @data[0,n]
    @data = @data[n..-1]

    return response
  end

  def write(data)
    puts("The server tried to send: #{data.unpack("H*")}")
  end

  def close()
    # do nothing
  end
end

test = Test.new

test.queue( [
              Packet::MESSAGE_TYPE_SYN,  # Type
              0x1234,            # Session id
              0x0000,            # Options
              0x0000,            # Initial seq
            ].pack("Cnnn"))

test.queue( [
              Packet::MESSAGE_TYPE_SYN, # Type
              0x4321,            # Session id
              0x0000,            # Options
              0x0000,            # Initial seq
            ].pack("Cnnn"))

test.queue( [
              Packet::MESSAGE_TYPE_MSG, # Type
              0x1234,            # Session id
              0x0000,            # SEQ
              0x0000,            # ACK
              "This is some incoming data"
            ].pack("CnnnA*"))

session = Session.find(0x1234)
session.queue_outgoing("This is some outgoing data queued up!")
Dnscat2.go(test)

#def get_socket_string(addr)
#  return "#{addr[3]}:#{addr[1]} (#{addr[2]})"
#end
#
#server = TCPServer.new(2000)
#
#puts("Server ready: #{get_socket_string(server.addr)}")
#loop do
#  Thread.start(server.accept) do |s|
#    begin
#      puts("Received a connection from #{get_socket_string(s.peeraddr)}")
#      go_tcp(s)
#    rescue Exception => e
#      puts(e.inspect)
#      puts(e.backtrace)
#    end
#  end
#end
