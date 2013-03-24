##
# dnscat2_server.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# Implements basically the full Dnscat2 protocol. Doesn't care about
# lower-level protocols.
##
require 'log'
require 'packet'
require 'session'

# This class should be totally stateless, and rely on the Session class
# for any long-term session storage
class Dnscat2
  def Dnscat2.handle_syn(pipe, packet, session)
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!session.syn_valid?())
      Log.log(session.id, "SYN invalid in this state (ignored)")
      return nil
    end

    Log.log(session.id, "Received SYN; responding with SYN")

    session.set_their_seq(packet.seq)
    session.set_established()

    return Packet.create_syn(session.id, session.my_seq, nil)
  end

  def Dnscat2.handle_msg(pipe, packet, session)
    if(!session.msg_valid?())
      Log.log(session.id, "MSG invalid in this state (responding with an error)")
      return Packet.create_fin(session.id)
    end

    # Validate the sequence number
    if(session.their_seq != packet.seq)
      Log.log(session.id, "Bad sequence number; expected 0x%04x, got 0x%04x [re-sending]" % [session.their_seq, packet.seq])

      # Re-send the last packet
      old_data = session.read_outgoing(pipe.max_packet_size - Packet.msg_header_size)
      return Packet.create_msg(session.id, session.my_seq, session.their_seq, old_data)
    end

    if(!session.valid_ack?(packet.ack))
      Log.log(session.id, "Impossible ACK received: 0x%04x, current SEQ is 0x%04x [re-sending]" % [packet.ack, session.my_seq])

      # Re-send the last packet
      old_data = session.read_outgoing(pipe.max_packet_size - Packet.msg_header_size)
      return Packet.create_msg(session.id, session.my_seq, session.their_seq, old_data)
    end

    # Acknowledge the data that has been received so far
    session.ack_outgoing(packet.ack)

    # Write the incoming data to the session
    session.queue_incoming(packet.data)

    # Increment the expected sequence number
    session.increment_their_seq(packet.data.length)

    new_data = session.read_outgoing(pipe.max_packet_size - Packet.msg_header_size)
    Log.log(session.id, "Received MSG with #{packet.data.length} bytes; responding with our own message (#{new_data.length} bytes)")
    Log.log(session.id, ">> \"#{packet.data}\"")
    Log.log(session.id, "<< \"#{new_data}\"")

    # Build the new packet
    return Packet.create_msg(session.id, session.my_seq, session.their_seq, new_data)
  end

  def Dnscat2.handle_fin(pipe, packet, session)
    # Ignore errant FINs - if we respond to a FIN with a FIN, it would cause a potential infinite loop
    if(!session.fin_valid?())
      Log.log(session.id, "FIN invalid in this state")
      return nil
    end

    Log.log(session.id, "Received FIN for session #{session.id}; closing session")
    session.destroy()
    return Packet.create_fin(session.id)
  end

  def Dnscat2.go(pipe)
    # Create a thread that listens on stdin and sends the data everywhere
    Thread.new do
      begin
        loop do
          Session.queue_all_outgoing(gets())
        end
      rescue Exception => e
        puts(e)
        exit
      end
    end

    begin
      if(pipe.max_packet_size < 16)
        raise(Exception, "max_packet_size is too small")
      end

      session_id = nil

      pipe.recv() do |data|
        packet = Packet.parse(data)

        # Store the session_id in a variable so we can close it if there's a problem
        session_id = packet.session_id

        session = Session.find(packet.session_id)

        response = nil
        if(packet.type == Packet::MESSAGE_TYPE_SYN)
          response = handle_syn(pipe, packet, session)
        elsif(packet.type == Packet::MESSAGE_TYPE_MSG)
          response = handle_msg(pipe, packet, session)
        elsif(packet.type == Packet::MESSAGE_TYPE_FIN)
          response = handle_fin(pipe, packet, session)
        else
          raise(IOError, "Unknown packet type: #{packet.type}")
        end

        if(response)
          if(response.length > pipe.max_packet_size)
            raise(IOError, "Tried to send packet longer than max_packet_length")
          end
        end

        response
      end
    rescue IOError => e
      # Don't destroy the session on IOErrors, a new connection can resume the same session
      raise(e)

    rescue Exception => e
      # Destroy the session on non-IOError exceptions
      begin
        if(!session_id.nil?)
          Log.log("ERROR", "Exception thrown; attempting to close session #{session_id}...")
          Session.destroy(session_id)
          Log.log("ERROR", "Propagating the exception...")
        end
      rescue
        # Do nothing
      end

      raise(e)
    end
  end
end
