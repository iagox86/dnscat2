##
# session.rb
# Created August 29, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
##

class DriverConsole
  def initialize(window)
    @window = window
    @outgoing = ""

    @window.on_input() do |data|
      @outgoing += data
    end

    @window.puts("This is a console session!")
    @window.puts("")
    @window.puts("That means that anything you type will be sent as-is to the")
    @window.puts("client, and anything they type will be displayed as-is on the")
    @window.puts("screen! If the client is executing a command and you don't")
    @window.puts("see a prompt, try typing 'pwd' or something!")
    @window.puts("")
    @window.puts("To exit, type ctrl-z.")
  end

  def feed(data)
    @window.puts(data)

    out = @outgoing
    @outgoing = ''

    return out
  end
end

class Test
  def on_input()
  end
  def puts(a)
  end
end
a = Test.new()

test = DriverConsole.new(a)
puts("+++ " + test.class.to_s)
