##
# controller.rb
# Created April, 2014
# By Ron Bowes
#
# See: LICENSE.md
#
# This keeps track of all the currently active sessions.
##

require 'controller/controller_commands'
require 'controller/packet'
require 'controller/session'
require 'libs/commander'
require 'libs/dnscat_exception'
require 'libs/log'

require 'trollop'

class Controller

  include ControllerCommands

  def initialize(window)
    @window = window
    @commander = Commander.new()
    @sessions = {}

    _register_commands()

    @window.on_input() do |data|
      @commander.feed(data)
    end
  end

  def _get_or_create_session(id)
    if(@sessions[id])
      return @sessions[id]
    end

    return (@sessions[id] = Session.new(id, @window))
  end

  def session_exists?(id)
    return !@sessions[id].nil?
  end

  def find_session(id)
    return @sessions[id]
  end

  def kill_session(id)
    session = find(id)

    if(!session.nil?)
      session.kill()
    end
  end

  def list()
    return @sessions
  end

  def feed(data, max_length)
    begin
      session_id = Packet.peek_session_id(data)

      session = _get_or_create_session(session_id)

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

