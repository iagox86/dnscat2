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
  @@subscribers = []
  @@sessions = {}

  attr_reader :id, :state, :their_seq, :my_seq
  attr_reader :name

  # Session states
  STATE_NEW         = 0x00
  STATE_ESTABLISHED = 0x01

  def SessionManager.create_session(id)
    session = Session.new(id)
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
      session = create_session(packet.session_id)
    end

    return session.handle_syn(packet)
  end

  def SessionManager.handle_msg(packet, max_length)
    session = find(packet.session_id)
    if(session.nil?)
      Log.WARNING("MSG received in non-existent session: %d" % packet.session_id)
      return Packet.create_fin(packet.session_id, "Bad session", 0)
    end

    return session.handle_msg(packet, max_length)
  end

  def SessionManager.handle_fin(packet)
    session = find(packet.session_id)

    if(session.nil?)
      Log.WARNING("FIN received in non-existent session: %d" % packet.session_id)
      return Packet.create_fin(packet.session_id, "Bad session", 0)
    end

    return session.handle_fin(packet)
  end

  def SessionManager.go(pipe)
    pipe.recv() do |data, max_length|
      session_id = nil

      begin
        packet = Packet.parse_header(data)
        session_id = packet.session_id # This is helpful if an exception is thrown
        session = find(session_id)

        # Parse the packet's body
        packet.parse_body(data, session.nil?() ? 0 : session.options)

        # Poke everybody else to let the know we're still seeing packets
        if(!session.nil?)
          session.notify_subscribers(:session_heartbeat, [session_id])
        end

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

        raise(e)
      end
    end
  end
end

