##
# driver_command.rb
# Created August 29, 2015
# By Ron Bowes
#
# See: LICENSE.md
##

require 'shellwords'

require 'drivers/driver_command_commands'
require 'libs/command_packet'

class DriverCommand
  include DriverCommandCommands

  def request_id()
    id = @request_id
    @request_id += 1
    return id
  end

  def _get_pcap_window()
    id = "cmdpcap#{@window.id}"

    if(SWindow.exists?(id))
      return SWindow.get(id)
    end

    return SWindow.new(@window, false, {
      :id => id,
      :name => "dnscat2 command protocol window for session #{@window.id}",
      :noinput => true,
    })
  end

  def _send_request(request)
    @handlers[request.get(:request_id)] = {
      :request => request,
      :proc => proc
    }

    if(Settings::GLOBAL.get("packet_trace"))
      window = _get_pcap_window()
      window.puts("OUT: #{request}")
    end

    out = request.serialize()
    out = [out.length, out].pack("Na*")
    @outgoing += out
  end

  def initialize(window, settings)
    @window = window
    @settings = settings
    @outgoing = ""
    @incoming = ""
    @request_id = 0x0001
    @commander = Commander.new()
    @handlers = {}

    _register_commands()

    @window.on_input() do |data|
      # This replaces any $variables with 
      data = @settings.do_replace(data)
      begin
        @commander.feed(data)
      rescue ArgumentError => e
        @window.puts("Error: #{e}")
        @commander.educate(data, @window)
      end
    end

    @window.puts("This is a command session!")
    @window.puts()
    @window.puts("That means you can enter a dnscat2 command such as")
    @window.puts("'ping'! For a full list of clients, try 'help'.")
    @window.puts()

    # This creates the window early for a slightly better UX (it doesn't pop up after their first command)
    if(Settings::GLOBAL.get("packet_trace"))
      window = _get_pcap_window()
    end

    # Sideload the auto-command if one is set (this should happen at the end of initialize())
#    if(auto_command = Settings::GLOBAL.get("auto_command"))
#      auto_command.split(";").each do |command|
#        command = command.strip()
#        @commander.feed(command + "\n")
#      end
#    end
  end

  def _handle_incoming(command_packet)
    if(Settings::GLOBAL.get("packet_trace"))
      window = _get_pcap_window()
      window.puts("IN:  #{command_packet}")
    end

    if(command_packet.get(:is_request))
      @window.puts("ERROR: The client sent us a request! That's not valid (but")
      @window.puts("it may be in the future, so this may be a version mismatch")
      @window.puts("problem)")

      return
    end

    if(@handlers[command_packet.get(:request_id)].nil?)
      @window.puts("Received a response that we have no record of sending:")
      @window.puts("#{command_packet}")
      @window.puts()
      @window.puts("Here are the responses we're waiting for:")
      @handlers.each_pair do |request_id, handler|
        @window.puts("#{request_id}: #{handler[:request]}")
      end

      return
    end

    handler = @handlers.delete(command_packet.get(:request_id))
    handler[:proc].call(handler[:request], command_packet)
  end

  def feed(data)
    @incoming += data
    loop do
      if(@incoming.length < 4)
        break
      end

      # Try to read a length + packet
      length, data = @incoming.unpack("Na*")

      # If there isn't enough data, give up
      if(data.length < length)
        break
      end

      # Otherwise, remove what we have from @data
      length, data, @incoming = @incoming.unpack("Na#{length}a*")
      _handle_incoming(CommandPacket.parse(data, false))
    end

    # Return the queue and clear it out
    result = @outgoing
    @outgoing = ''
    return result
  end
end
