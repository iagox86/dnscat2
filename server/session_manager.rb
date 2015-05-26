##
# session_manager.rb
# Created April, 2014
# By Ron Bowes
#
# See: LICENSE.md
#
# This keeps track of all the currently active sessions.
##

require 'dnscat_exception'
require 'log'
require 'packet'
require 'subscribable'
require 'session'

class SessionManager
  @@subscribers = []
  @@sessions = {}

  def SessionManager.create_session(id)
    session = Session.new(id)
    session.subscribe(@@subscribers)
    @@sessions[id] = session

    return session
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
      session.kill()
    end
  end

  def SessionManager.list()
    return @@sessions
  end

  def SessionManager.destroy()
    Log.FATAL(nil, "TODO: Implement destroy()")
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
      err = "MSG received in non-existent session: %d" % packet.session_id

      Log.ERROR(packet.session_id, err)
      return Packet.create_fin(0, {
        :session_id => packet.session_id,
        :reason     => err,
      })
    end

    return session.handle_msg(packet, max_length)
  end

  def SessionManager.handle_fin(packet)
    session = find(packet.session_id)

    if(session.nil?)
      err = "FIN received in non-existent session: %d" % packet.session_id

      Log.ERROR(packet.session_id, err)
      return Packet.create_fin(0, {
        :session_id => packet.session_id,
        :reason     => err,
      })
    end

    return session.handle_fin(packet)
  end

  def SessionManager.handle_ping(packet)
    return packet
  end

  def SessionManager.go(pipe, settings)
    pipe.recv() do |data, max_length|
      session_id = nil

      begin
        # Get the options for the session - this is a bit hacky
        session_id = Packet.peek_session_id(data)
        session = find(session_id)
        options = session.nil? ? 0 : session.options

        # Parse the packet
        packet = Packet.parse(data, options)


        # Poke everybody else to let the know we're still seeing packets
        # TODO: Do I care?
        if(!session.nil?)
          session.notify_subscribers(:session_heartbeat, [session_id])
        end

        response = nil
        if(packet.type == Packet::MESSAGE_TYPE_SYN)
          response = handle_syn(packet)
        end

        # Display the incoming packet (NOTE: this has to be done *after* handle_syn(), otherwise
        # the message doesn't have a session to go to)
        if(settings.get("packet_trace"))
          Log.PRINT(session_id, "INCOMING: #{packet.to_s}")
        end

        if(packet.type == Packet::MESSAGE_TYPE_SYN)
          # Already handled
        elsif(packet.type == Packet::MESSAGE_TYPE_MSG)
          response = handle_msg(packet, max_length)
        elsif(packet.type == Packet::MESSAGE_TYPE_FIN)
          response = handle_fin(packet)
        elsif(packet.type == Packet::MESSAGE_TYPE_PING)
          response = handle_ping(packet)
        else
          raise(DnscatException, "Unknown packet type: #{packet.type}")
        end


        # If there's a response, validate it
        if(!response.nil?)
          if(response.to_bytes().length > max_length)
            raise(DnscatException, "Tried to send packet of #{response.to_bytes().length} bytes, but max_length is #{max_length} bytes")
          end
        end

        # Show the response, if requested
        if(settings.get("packet_trace"))
          Log.PRINT(session_id, "OUTGOING: #{response.to_s}")
        end

        if(response.nil?)
          nil
        else
          response.to_bytes() # Return it, in a way
        end

      # Catch IOErrors, but don't destroy the session - it may continue later
      rescue IOError => e
        Log.ERROR(session_id, e)
        raise(e)

      # Destroy the session on protocol errors - the client will be informed if they
      # send another message, because they'll get a FIN response
      rescue Exception => e
        Log.ERROR(session_id, e)

        begin
          if(!session_id.nil?)
            Log.ERROR(session_id, "DnscatException caught; closing session #{session_id}...")
            kill_session(session_id)
          end
        rescue => e2
          Log.ERROR(session_id, "Error closing session!")
          Log.ERROR(session_id, e2)
        end

        Log.ERROR(session_id, "Propagating the exception...")

        raise(e)
      end
    end
  end
end

