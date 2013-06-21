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
      print("dnscat [#{@@session.id}]> ")
    end
  end

  def Ui.do(command, args)
    if(command == "")
      # Do nothing
    elsif(command == "sessions")
      puts("Sessions:")
      puts(Session.list())
    elsif(command == "session")
      if(args[0] == "-i") # TODO: Parse this in a less retarded way
        id = args[1].to_i
        session = Session.find(id)
        if(session.nil?)
          puts("Session #{id} not found! Try 'sessions' to get a list")
        else
          puts("Interacting with session: #{session.id}")
          @@session = session
        end
      else
        puts("Unknown option to 'session': #{args[1]}")
      end
    elsif(command == "help")
      puts("Help!")
    elsif(command == "exit")
      puts("Bye!")
      exit
    else
      puts("Unknown command: #{command}")
    end
  end

  def Ui.process_line(line)
    if(@@session.nil?)
      split = line.split(/ /)

      if(split.length > 0)
        command = split.shift
        args = split
      else
        command = ""
        args = nil
      end

      Ui.do(command, args)
    else
      @@session.queue_outgoing(line)
    end
  end

  def Ui.go()
    Ui.prompt()
    loop do
      line = nil
      if(!@@session.nil?)
        result = IO.select([$stdin], nil, nil, 0.25)
        if(!result.nil? && result.length > 0)
          line = $stdin.readline.chomp
        end
      else
        line = $stdin.readline.chomp
      end

      # Check if we have any data typed by the user
      if(!line.nil?)
        Ui.process_line(line)
        Ui.prompt()
      end

      if(!@@session.nil? && @@session.incoming?)
        Ui.display(@@session.read_incoming)
      end
    end
  end

  def Ui.display(message)
    puts()
    puts(message)
    prompt()
  end
end

Signal.trap("TSTP") do
  Ui.detach
end
