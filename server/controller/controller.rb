##
# session_manager.rb
# Created April, 2014
# By Ron Bowes
#
# See: LICENSE.md
#
# This keeps track of all the currently active sessions.
##

require 'controller/packet'
require 'controller/session'
require 'libs/commander'
require 'libs/dnscat_exception'
require 'libs/log'

require 'trollop'

class Controller
  @@sessions = {}

  def Controller.create_session(id)
    @@sessions[id] = Session.new()
  end

  def Controller.session_exists?(id)
    return !@@sessions[id].nil?
  end

  def Controller.find_session(id)
    return @@sessions[id]
  end

  def Controller.kill_session(id)
    session = find(id)

    if(!session.nil?)
      session.kill()
    end
  end

  def Controller.list()
    return @@sessions
  end

  def Controller.feed(data, max_length)
    begin
      session_id = Packet.peek_session_id(data)

      session = Controller.find_session(session_id)
      if(session.nil?)
        Log.WARNING(nil, "Data came for unknown session: #{session_id}")
        return nil
      end

      return session.feed(data, max_length)
    rescue DnscatException => e
      Log.ERROR(session_id, e)

      if(!session.nil?)
        Log.ERROR(session_id, "DnscatException caught; closing session #{session_id}...")
        kill_session(session_id)
      end
    end
  end
end

