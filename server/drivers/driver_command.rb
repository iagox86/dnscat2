##
# driver_command.rb
# Created August 29, 2015
# By Ron Bowes
#
# See: LICENSE.md
##

require 'shellwords'

require 'drivers/command_packet'
require 'drivers/driver_command_commands'
require 'drivers/driver_command_tunnels'

class DriverCommand
  include DriverCommandCommands
  include DriverCommandTunnels

  attr_reader :stopped

  @@mutex = Mutex.new()

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

  def _send_request(request, callback)
    # Make sure this is synchronous so threads don't fight
    @@mutex.synchronize() do
      if(callback)
        @handlers[request.get(:request_id)] = {
          :request => request,
          :proc => callback
        }
      end

      if(Settings::GLOBAL.get("packet_trace"))
        window = _get_pcap_window()
        window.puts("OUT: #{request}")
      end

      out = request.serialize()
      out = [out.length, out].pack("Na*")
      @outgoing += out
    end
  end

  def initialize(window, settings)
    @window = window
    @settings = settings
    @outgoing = ""
    @incoming = ""
    @request_id = 0x0001
    @commander = Commander.new()
    @handlers = {}
    @stopped = false

    _register_commands()
    _register_commands_tunnels()

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
  end

  def _handle_incoming(command_packet)
    if(Settings::GLOBAL.get("packet_trace"))
      window = _get_pcap_window()
      window.puts("IN:  #{command_packet}")
    end

    if(command_packet.get(:is_request))
      tunnel_data_incoming(command_packet)
      return
    end

    handler = @handlers.delete(command_packet.get(:request_id))
    if(handler.nil?)
      @window.puts("Received a response that we have no record of sending:")
      @window.puts("#{command_packet}")
      @window.puts()
      @window.puts("Here are the responses we're waiting for:")
      @handlers.each_pair do |request_id, the_handler|
        @window.puts("#{request_id}: #{the_handler[:request]}")
      end
      return
    end

    if(command_packet.get(:command_id) == CommandPacket::COMMAND_ERROR)
      @window.puts("Client returned an error: #{command_packet.get(:status)} :: #{command_packet.get(:reason)}")
      return
    end

    if(handler[:request].get(:command_id) != command_packet.get(:command_id))
      @window.puts("Received a response of a different packet type (that's really weird, please report if you can reproduce!")
      @window.puts("#{command_packet}")
      @window.puts()
      @window.puts("The original packet was:")
      @window.puts("#{handler}")
      return
    end

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
      _handle_incoming(CommandPacket.parse(data))
    end

    # Return the queue and clear it out
    result = @outgoing
    @outgoing = ''
    return result
  end

  def request_stop()
    @stopped = true
  end

  def shutdown()
    tunnels_stop()
  end
end
