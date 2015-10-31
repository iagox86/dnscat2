##
# controller.rb
# Created April, 2014
# By Ron Bowes
#
# See: LICENSE.md
#
# This keeps track of all sessions.
##

require 'controller/controller_commands'
require 'controller/packet'
require 'controller/session'
require 'libs/commander'
require 'libs/dnscat_exception'

require 'trollop'

class Controller
  include ControllerCommands

  attr_accessor :window

  def initialize()
    @commander = Commander.new()
    @sessions = {}

    _register_commands()

    WINDOW.on_input() do |data|
      data = Settings::GLOBAL.do_replace(data)
      begin
        @commander.feed(data)
      rescue ArgumentError => e
        WINDOW.puts("Error: #{e}")
        WINDOW.puts()
        @commander.educate(data, WINDOW)
      end
    end
  end

  def _get_or_create_session(id)
    if(@sessions[id])
      return @sessions[id]
    end

    return (@sessions[id] = Session.new(id, WINDOW))
  end

  def session_exists?(id)
    return !@sessions[id].nil?
  end

  def find_session(id)
    return @sessions[id]
  end

  def find_session_by_window(id)
    id = id.to_s()
    @sessions.each_value do |session|
      if(session.window.id.to_s() == id)
        return session
      end
    end

    return nil
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
    # If it's a ping packet, handle it up here
    if(Packet.peek_type(data) == Packet::MESSAGE_TYPE_PING)
      WINDOW.puts("Responding to ping packet: #{Packet.parse(data).body}")
      return data
    end

    session_id = Packet.peek_session_id(data)
    session = _get_or_create_session(session_id)

    return session.feed(data, max_length)
  end
end
