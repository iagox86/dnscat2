##
# driver_console.rb
# Created August 29, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
##

class DriverConsole
  def initialize(window, settings)
    @window = window
    @settings = settings
    @outgoing = ""

    @window.on_input() do |data|
      @outgoing += data
      @outgoing += "\n"
    end

    @window.puts("This is a console session!")
    @window.puts()
    @window.puts("That means that anything you type will be sent as-is to the")
    @window.puts("client, and anything they type will be displayed as-is on the")
    @window.puts("screen! If the client is executing a command and you don't")
    @window.puts("see a prompt, try typing 'pwd' or something!")
    @window.puts()
    @window.puts("To go back, type ctrl-z.")
    @window.puts()
  end

  def feed(data)
    @window.print(data)

    out = @outgoing
    @outgoing = ''

    return out
  end
end
