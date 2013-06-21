##
# ui.rb
# Created June 20, 2013
# By Ron Bowes
##

require 'session'

class Ui
  @@session = nil

  def Ui.detach()
    @@session = nil
    Ui.prompt()
  end

  def Ui.prompt()
    if(@@session.nil?)
      print("dnscat> ")
    else
      print("dnscat [#{@@session}]> ")
    end
  end

  def Ui.do(command, args)
    if(@@session.nil?)
      if(command == "")
        # Do nothing
      elsif(command == "sessions")
        puts("Sessions:")
        puts(Session.list())
      elsif(command == "help")
        puts("Help!")
      elsif(command == "exit")
        puts("Bye!")
        exit
      else
        puts("Unknown command: #{command.class}")
      end
    else
      puts("No commands in-session yet")
    end
  end

  def Ui.go()
    loop do
      prompt()

      #IO.select([$stdin], nil, nil, nil)
      line = $stdin.readline.chomp
      split = line.split(/ /)

      if(split.length > 0)
        command = split.shift
        args = split
      else
        command = ""
        args = nil
      end

      Ui.do(command, args)
    end
  end

  def Ui.status(message)
    puts()
    puts(message)
    prompt()
  end
end

Signal.trap("TSTP") do
  Ui.detach
end
