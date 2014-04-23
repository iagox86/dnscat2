##
# session_manager.rb
# Created April, 2014
# By Ron Bowes
#
# See: LICENSE.txt
#
# This keeps track of all the currently active sessions.
##

require 'log'
require 'dnscat_exception'
require 'subscribable'
require 'session'

class SessionManager
  @@tunnels = {}
  @@subscribers = []
  @@sessions = {}

  attr_reader :id, :state, :their_seq, :my_seq
  attr_reader :name

  # Session states
  STATE_NEW         = 0x00
  STATE_ESTABLISHED = 0x01

  def SessionManager.create_session(id, datagram = false)
    session = Session.new(id, datagram)
    session.subscribe(@@subscribers)
    @@sessions[id] = session
  end

  def SessionManager.subscribe(cls)
    @@subscribers << cls
  end

  def SessionManager.exists?(id)
    return !@@sessions[id].nil?
  end

  def SessionManager.find(id)
    return @@sessions[id]
  end

  def SessionManager.kill_session(id)
    # Notify subscribers before deleting it, in case they want to do something with
    # it first
    session = find(id)
    if(!session.nil?)
      session.notify_subscribers(:session_destroyed, [id])

      if(!@@tunnels[session.id].nil?)
        @@tunnels[session.id].kill
      end

      @@sessions.delete(id)
    end
  end

  def SessionManager.list()
    return @@sessions
  end

  def SessionManager.destroy()
    Log.ERROR("TODO: Implement destroy()")
    #Session.destroy(@id)
  end

  def to_s()
    return "id: 0x%04x, state: %d, their_seq: 0x%04x, my_seq: 0x%04x, incoming_data: %d bytes [%s], outgoing data: %d bytes [%s]" % [@id, @state, @their_seq, @my_seq, @incoming_data.length, @incoming_data, @outgoing_data.length, @outgoing_data]
  end

  def SessionManager.handle_syn(packet)
    session = find(packet.session_id)

    if(session.nil?)
      # If the session doesn't exist, and it's a SYN, create it
      session = create_session(packet.session_id, packet.datagram)
    end

    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!session.syn_valid?())
      session.notify_subscribers(:dnscat2_state_error, [session.id, "SYN received in invalid state"])
      return nil
    end

    session.set_their_seq(packet.seq)
    session.set_name(packet.name)
    session.set_established()

    if(!packet.tunnel_host.nil?)
      begin
        Log.WARNING("Creating a tunnel to #{packet.tunnel_host}:#{packet.tunnel_port}")
        @@tunnels[session.id] = Tunnel.new(session.id, packet.tunnel_host, packet.tunnel_port)
      rescue Exception => e
        Log.ERROR("Couldn't create a tunnel: #{e}")
        kill_session(session.id)
        return Packet.create_fin(packet.packet_id, session.id)
      end
    end

    session.notify_subscribers(:dnscat2_syn_received, [session.id, session.my_seq, packet.seq])

    return Packet.create_syn(packet.packet_id, session.id, session.my_seq, nil)
  end

  def SessionManager.handle_msg(packet, max_length)
    session = find(packet.session_id)
    if(session.nil?)
      Log.WARNING("MSG received in non-existent session: %d" % packet.session_id)
      return Packet.create_fin(packet.packet_id, packet.session_id)
    end

    if(!session.msg_valid?())
      session.notify_subscribers(:dnscat2_state_error, [session.id, "MSG received in invalid state; sending FIN"])

      # Kill the session as well - in case it exists
      kill_session(session.id)

      return Packet.create_fin(packet.packet_id, session.id)
    end

    # Don't bother with any of this in datagram mode
    if(!session.datagram)
      # Validate the sequence number
      if(session.their_seq != packet.seq)
        session.notify_subscribers(:dnscat2_msg_bad_seq, [session.their_seq, packet.seq])

        # Re-send the last packet
        old_data = session.read_outgoing(max_length - Packet.msg_header_size)
        return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, old_data)
      end

      if(!session.valid_ack?(packet.ack))
        session.notify_subscribers(:dnscat2_msg_bad_ack, [session.my_seq, packet.ack])

        # Re-send the last packet
        old_data = session.read_outgoing(max_length - Packet.msg_header_size)
        return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, old_data)
      end

      # Acknowledge the data that has been received so far
      # Note: this is where @my_seq is updated
      session.ack_outgoing(packet.ack)
    end

    # Write the incoming data to the session
    session.queue_incoming(packet.data)

    if(!session.datagram)
      # Increment the expected sequence number
      session.increment_their_seq(packet.data.length)
    end

    # Send the data through a tunnel, if necessary
    if(!@@tunnels[session.id].nil?)
      # Send the data on if it's a tunnel
      @@tunnels[session.id].send(packet.data)
    end

    new_data = session.read_outgoing(max_length - Packet.msg_header_size)
    session.notify_subscribers(:dnscat2_msg, [packet.data, new_data])

    # Build the new packet
    return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, new_data)
  end

  def SessionManager.handle_fin(packet)
    session = find(packet.session_id)

    if(session.nil?)
      Log.WARNING("FIN received in non-existent session: %d" % packet.session_id)
      return Packet.create_fin(packet.packet_id, packet.session_id)
    end

    # Ignore errant FINs - if we respond to a FIN with a FIN, it would cause a potential infinite loop
    if(!session.fin_valid?())
      session.notify_subscribers(:dnscat2_state_error, [session.id, "FIN received in invalid state"])
      return Packet.create_fin(packet.packet_id, session.id)
    end

    session.notify_subscribers(:dnscat2_fin, [session.id])
    kill_session(session.id)

    return Packet.create_fin(packet.packet_id, session.id)
  end

  def SessionManager.go(pipe)
    pipe.recv() do |data, max_length|
      session_id = nil

      begin
        packet = Packet.parse(data)
        session_id = packet.session_id # This is helpful if an exception is thrown
        session = find(session_id)

        #session.notify_subscribers(:dnscat2_recv, [packet])

        response = nil
        if(packet.type == Packet::MESSAGE_TYPE_SYN)
          response = handle_syn(packet)
        elsif(packet.type == Packet::MESSAGE_TYPE_MSG)
          response = handle_msg(packet, max_length)
        elsif(packet.type == Packet::MESSAGE_TYPE_FIN)
          response = handle_fin(packet)
        else
          raise(DnscatException, "Unknown packet type: #{packet.type}")
        end

        if(!response.nil?)
          # notify_subscribers(:dnscat2_send, [Packet.parse(response)])
          if(response.length > max_length)
            raise(RuntimeError, "Tried to send packet of #{response.length} bytes, but max_length is #{max_length} bytes")
          end
        end

        response # Return it, in a way

      # Catch IOErrors, but don't destroy the session - it may continue later
      rescue IOError => e
        Log.ERROR("Caught IOError signal")
        raise(e)

      # Destroy the session on protocol errors - the client will be informed if they
      # send another message, because they'll get a FIN response
      rescue DnscatException => e
        begin
          if(!session_id.nil?)
            Log.FATAL("DnscatException caught; closing session #{session_id}...")
            kill_session(session.id)
            Log.FATAL("Propagating the exception...")
          end
        rescue
          # Do nothing
        end

        raise(e)
      rescue Exception => e
        Log.FATAL("Exception: #{e}")
        Log.FATAL(e.inspect)
        Log.FATAL(e.backtrace)
      end
      # Let other exceptions propagate, they will be displayed by the parent
    end
  end
end

