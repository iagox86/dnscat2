##
# ui.rb
# Created June 20, 2013
# By Ron Bowes
##

require 'trollop' # We use this to parse commands

class Ui
  MAX_CHARACTERS = 4096

  @@session = nil
  @@data = {}
  @@options = {}

  def Ui.detach()
    @@session = nil
    Ui.prompt()
  end

  def Ui.set_option(name, value)
    @@options[name] = value
  end

  def Ui.prompt()
    if(@@session.nil?)
      print("dnscat> ")
    else
      print("dnscat [#{@@session.id}]> ")
    end
  end

  def Ui.attach(id)
    session = Session.find(id)
    puts("Interacting with session: #{session.id}")
    if(!@@data[session.id].nil?)
      puts(@@data[session.id])
    end
    @@session = session

    prompt()
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
          Ui.attach(opts[:id])
        end
      end
    },

    "set" => {
      :parser => Trollop::Parser.new do
        banner("With no arguments, displays the current settings; can also set using the argument 'name=value'")
      end,

      :proc => Proc.new do |opts, optarg|
        puts("opts = #{opts} #{opts.class}")
        if(optarg.length == 0)
          @@options.each_pair do |name, value|
            puts("#{name} => #{value}")
          end
        else
          optarg = optarg.join(" ")

          # Split at the '=' sign
          optarg = optarg.split("=", 2)

          # If we don't have a name=value setup, show an error
          if(optarg.length != 2)
            puts("Usage: set <name>=<value>")
          else
            @@options[optarg[0]] = optarg[1]
            puts("#{optarg[0]} => #{optarg[1]}")
          end
        end
      end
    }
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
        command[:proc].call(opts, args)
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
    # Subscribe to the session to get updates
    Session.subscribe(Ui)

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

  def Ui.session_created(id)
  end

  def Ui.session_established(id)
    # TODO
    puts("New session established: #{id}")
    if(!@@options[:auto_command].nil?)
      Session.find(id).queue_outgoing(@@options[:auto_command])
    end

    if(@@session.nil?() && @@options[:auto_attach])
      Ui.attach(id)
    end
  end

  def Ui.data_received(id, data)
    # TODO: Limit the length
    @@data[id] = @@data[id] || ""
    @@data[id] += data

    # If we're currently watching this session, display the data
    if(!@@session.nil? && @@session.id == id)
      puts(data)
      prompt()
    end
  end

  def Ui.outgoing_sent(ret)
  end

  def Ui.outgoing_acknowledged(data)
  end

  def Ui.outgoing_queued(data)
  end

  def Ui.session_destroyed(id)
  end
end

# Trap ctrl-z, just like Metasploit
Signal.trap("TSTP") do
  puts()
  Ui.detach
end
