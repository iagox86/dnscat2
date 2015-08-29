##
# driver_command.rb
# Created August 29, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
##

class DriverCommand
  def register_commands()
    @commander.register_command('echo',
      Trollop::Parser.new do
        banner("Print stuff to the terminal")
      end,

      Proc.new do |opts, optval|
        @window.puts(optval)
      end
    )
  end

  def initialize(window)
    @window = window
    @outgoing = ""
    @commander = Commander.new()
    register_commands()

    @window.on_input() do |data|
      @commander.feed(data)
    end

    @window.puts("This is a command session!")
    @window.puts()
    @window.puts("That means you can enter a dnscat2 command such as")
    @window.puts("'ping'! For a full list of clients, try 'help'.")
    @window.puts()
  end

  def feed(data)
    # TODO: Parse data coming in from the network
    return ''
  end
end
