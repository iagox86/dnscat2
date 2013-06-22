##
# ui.rb
# Created June 20, 2013
# By Ron Bowes
##

require 'trollop' # We use this to parse commands

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

  COMMANDS = {
    "" => {
      :parser => Trollop::Parser.new do end,
      :proc => Proc.new do |opts| end,
    },

    "exit" => {
      :parser => Trollop::Parser.new do
        banner("Exits dnscat2")
      end,

      :proc => Proc.new do |opts| exit end,
    },

    "quit" => {
      :parser => Trollop::Parser.new do
        banner("Exits dnscat2")
      end,

      :proc => Proc.new do |opts| exit end,
    },

    "help" => {
      :parser => Trollop::Parser.new do
        banner("Shows a help menu")
      end,

      :proc => Proc.new do |opts|
        puts("Here are the available commands, listed alphabetically:")
        COMMANDS.keys.sort.each do |name|
          # Don't display the empty command
          if(name != "")
            puts("- #{name}")
          end
        end

        puts("For more information, --help can be passed to any command")
      end,
    },

    "clear" => {
      :parser => Trollop::Parser.new do end,
      :proc => Proc.new do |opts|
        0.upto(1000) do puts() end
      end,
    },

    "sessions" => {
      :parser => Trollop::Parser.new do
        banner("Lists the current active sessions")
        # No args
      end,

      :proc => Proc.new do |opts|
        puts("Sessions:")
        puts(Session.list())
      end,
    },

    "session" => {
      :parser => Trollop::Parser.new do
        banner("Handle interactions with a particular session (when in interactive mode, use ctrl-z to return to dnscat2)")
        opt :id, "Session id", :type => :integer, :required => true
      end,

      :proc => Proc.new do |opts|
        if(!Session.exists?(opts[:id]))
          Ui.error("Session #{opts[:id]} not found, run 'sessions' for a list")
        else
          session = Session.find(opts[:id])
          puts("Interacting with session: #{session.id}")
          @@session = session
        end
      end
    },

  }

  def Ui.process_line(line)
    split = line.split(/ /)

    if(split.length > 0)
      command = split.shift
      args = split
    else
      command = ""
      args = ""
    end

    if(COMMANDS[command].nil?)
      puts("Unknown command: #{command}")
    else
      begin
        command = COMMANDS[command]
        opts = command[:parser].parse(args)
        command[:proc].call(opts)
      rescue Trollop::CommandlineError => e
        Ui.error("ERROR: #{e}")
      rescue Trollop::HelpNeeded => e
        command[:parser].educate
      rescue Trollop::VersionNeeded => e
        Ui.error("Version needed!")
      end
    end
  end

  def Ui.go()
    Ui.prompt()
    loop do
      line = nil
      if(!@@session.nil?)
        result = IO.select([$stdin], nil, nil, 0.25)
        if(!result.nil? && result.length > 0)
          line = $stdin.readline
        end
      else
        line = $stdin.readline
      end

      # Check if we have any data typed by the user
      if(!line.nil?)
        if(@@session.nil?)
          line.chomp!()
          Ui.process_line(line)
        else
          if(line.length >= 12)
            puts("Too long!") # TODO: Fix this
          else
            @@session.queue_outgoing(line)
          end
        end

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

  def Ui.error(message)
    $stderr.puts(message)
  end
end

Signal.trap("TSTP") do
  puts()
  Ui.detach
end
