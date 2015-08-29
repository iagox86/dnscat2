##
# driver_command.rb
# Created August 29, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'drivers/command_packet'
require 'drivers/command_packet_stream'

class DriverCommand
  def request_id()
    id = @request_id
    @request_id += 1
    return id
  end
  
  def register_commands()
    @commander.register_command('echo',
      Trollop::Parser.new do
        banner("Print stuff to the terminal")
      end,

      Proc.new do |opts, optval|
        @window.puts(optval)
      end
    )

    @commander.register_command("ping",
      Trollop::Parser.new do
        banner("Sends a 'ping' to the remote host to make sure it's still alive)")
        opt :length, "length", :type => :integer, :required => false, :default => 1024
      end,
      Proc.new do |opts|
        data = (0...opts[:length]).map { ('A'.ord + rand(26)).chr }.join
        id = request_id()
        @pings[id] = data
        @outgoing += CommandPacket.create_ping_request(id, data)

        @window.puts("Ping request 0x%x sent! (0x%x bytes)" % [id, opts[:length]])
      end,
    )
  end

  def initialize(window)
    @window = window
    @outgoing = ""
    @incoming = ""
    @request_id = 0x0000
    @commander = Commander.new()
    register_commands()

    @pings = []

    @window.on_input() do |data|
      @commander.feed(data)
    end

    @window.puts("This is a command session!")
    @window.puts()
    @window.puts("That means you can enter a dnscat2 command such as")
    @window.puts("'ping'! For a full list of clients, try 'help'.")
    @window.puts()
  end

  def _handle_incoming(command_packet)
    @window.puts("Received: #{command_packet}")
  end

  def feed(data)
    @incoming += data

    loop do
      if(@incoming.length < 4)
        break
      end

      length, data = @incoming.unpack("Na*")
      if(data.length < length)
        return ''
      end

      _, @incoming = data.unpack("a#{length}a*")

      _handle_incoming(CommandPacket.new(data, false))
    end

    result = @outgoing
    @outgoing = ''
    return result
  end
end
