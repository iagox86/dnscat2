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

$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'driver_dns'
require 'driver_tcp'

require 'log'
require 'packet'
require 'session'
require 'tunnel'
require 'ui'

# Option parsing
require 'trollop'

# This class should be totally stateless, and rely on the Session class
# for any long-term session storage
class Dnscat2
  @@tunnels = {}

  # Begin subscriber stuff (this should be in a mixin, but static stuff doesn't
  # really seem to work
  @@subscribers = []
  def Dnscat2.subscribe(cls)
    @@subscribers << cls
  end
  def Dnscat2.unsubscribe(cls)
    @@subscribers.delete(cls)
  end
  def Dnscat2.notify_subscribers(method, args)
    @@subscribers.each do |subscriber|
      if(subscriber.respond_to?(method))
         subscriber.method(method).call(*args)
      end
    end
  end
  # End subscriber stuff

  def Dnscat2.kill_session(session)
    if(!@@tunnels[session.id].nil?)
      @@tunnels[session.id].kill
    end

    session.destroy
  end

  def Dnscat2.handle_syn(pipe, packet, session)
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!session.syn_valid?())
      Dnscat2.notify_subscribers(:dnscat2_state_error, [session.id, "SYN received in invalid state"])
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
        Dnscat2.kill_session(session)
        return Packet.create_fin(packet.packet_id, session.id)
      end
    end

    Dnscat2.notify_subscribers(:dnscat2_syn_received, [session.id, session.my_seq, packet.seq])

    return Packet.create_syn(packet.packet_id, session.id, session.my_seq, nil)
  end

  def Dnscat2.handle_msg(pipe, packet, session, max_length)
    if(!session.msg_valid?())
      Dnscat2.notify_subscribers(:dnscat2_state_error, [session.id, "MSG received in invalid state; sending FIN"])

      # Kill the session as well - in case it exists
      Dnscat2.kill_session(session)

      return Packet.create_fin(packet.packet_id, session.id)
    end

    # Validate the sequence number
    if(session.their_seq != packet.seq)
      Dnscat2.notify_subscribers(:dnscat2_msg_bad_seq, [session.their_seq, packet.seq])

      # Re-send the last packet
      old_data = session.read_outgoing(max_length - Packet.msg_header_size)
      return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, old_data)
    end

    if(!session.valid_ack?(packet.ack))
      Dnscat2.notify_subscribers(:dnscat2_msg_bad_ack, [session.my_seq, packet.ack])

      # Re-send the last packet
      old_data = session.read_outgoing(max_length - Packet.msg_header_size)
      return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, old_data)
    end

    # Acknowledge the data that has been received so far
    # Note: this is where @my_seq is updated
    session.ack_outgoing(packet.ack)

    # Write the incoming data to the session
    session.queue_incoming(packet.data)

    # Increment the expected sequence number
    session.increment_their_seq(packet.data.length)

    # Send the data through a tunnel, if necessary
    if(!@@tunnels[session.id].nil?)
      # Send the data on if it's a tunnel
      @@tunnels[session.id].send(packet.data)
    end

    new_data = session.read_outgoing(max_length - Packet.msg_header_size)
    Dnscat2.notify_subscribers(:dnscat2_msg, [packet.data, new_data])

    # Build the new packet
    return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, new_data)
  end

  def Dnscat2.handle_fin(pipe, packet, session)
    # Ignore errant FINs - if we respond to a FIN with a FIN, it would cause a potential infinite loop
    if(!session.fin_valid?())
      Dnscat2.notify_subscribers(:dnscat2_state_error, [session.id, "FIN received in invalid state"])
      return nil
    end

    Dnscat2.notify_subscribers(:dnscat2_fin, [session.id])

    Dnscat2.kill_session(session)
    return Packet.create_fin(packet.packet_id, session.id)
  end

  def Dnscat2.go(pipe)
    pipe.recv() do |data, max_length|
      session_id = nil

      begin
        packet = Packet.parse(data)

        Dnscat2.notify_subscribers(:dnscat2_recv, [packet])

        # Store the session_id in a variable so we can close it if there's a problem
        session_id = packet.session_id

        # Find the session
        session = Session.find(packet.session_id)

        puts("session: #{session.to_s}")

        response = nil
        if(session.nil?)
          if(packet.type == Packet::MESSAGE_TYPE_SYN)
            # If the session doesn't exist, and it's a SYN, create it
            session = Session.create_session(packet.session_id)
          else
            # If the session doesn't exist
            response = Packet.create_fin(packet.packet_id, packet.session_id)
          end
        end

        if(!session.nil?)
          if(packet.type == Packet::MESSAGE_TYPE_SYN)
            response = handle_syn(pipe, packet, session)
          elsif(packet.type == Packet::MESSAGE_TYPE_MSG)
            response = handle_msg(pipe, packet, session, max_length)
          elsif(packet.type == Packet::MESSAGE_TYPE_FIN)
            response = handle_fin(pipe, packet, session)
          else
            raise(DnscatException, "Unknown packet type: #{packet.type}")
          end
        end

        if(response)
          Dnscat2.notify_subscribers(:dnscat2_send, [Packet.parse(response)])
          if(response.length > max_length)
            raise(RuntimeError, "Tried to send packet of #{response.length} bytes, but max_length is #{max_length} bytes")
          end
        end

        response # Return it, in a way

      rescue SystemExit
        Log.FATAL("Caught EXIT signal, exiting")
        exit

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
            Dnscat2.kill_session(session)
            Log.FATAL("Propagating the exception...")
          end
        rescue
          # Do nothing
        end

        raise(e)
      rescue Exception => e
        Log.FATAL("Fatal exception caught:")
        Log.FATAL(e.inspect)
        Log.FATAL(e.backtrace)

        exit
      end
      # Let other exceptions propagate, they will be displayed by the parent
    end
  end
end

