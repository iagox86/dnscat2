##
# ui.rb
# Created June 20, 2013
# By Ron Bowes
##

require 'trollop' # We use this to parse commands

# Notification functions that are tied to a particular session:
# - session_created(id)
# - session_established(id)
# - session_data_received(id, data)
# - session_data_sent(id, data)
# - session_data_acknowledged(id, data)
# - session_data_queued(id, data)
# - session_destroyed(id)
#
# Calls that aren't tied to a session:
# - dnscat2_syn_received(my_seq, their_seq)
# - dnscat2_msg_bad_seq(expected_seq, received_seq)
# - dnscat2_msg_bad_ack(expected_ack, received_ack)
# - dnscat2_msg(incoming, outgoing)
# - dnscat2_fin()
# - dnscat2_recv(packet)
# - dnscat2_send(packet)


class Ui
  @@session = nil
  @@data = {}
  @@options = {}

  def Ui.detach()
    @@session = nil
    Ui.prompt()
  end

  def Ui.set_option(name, value)
    # Remove whitespace
    name  = name.to_s
    value = value.to_s

    name   = name.gsub(/^ */, '').gsub(/ *$/, '')
    value = value.gsub(/^ */, '').gsub(/ *$/, '')

    if(value == "nil")
      @@options.delete(name)

      puts("#{name} => [deleted]")
    else

      # Replace \n with actual newlines
      value = value.gsub(/\\n/, "\n")

      # Replace true/false with the proper values
      value = true if(value == "true")
      value = false if(value == "false")

      # Validate the log level
      if(name == "log_level" && Log.get_by_name(value).nil?)
        puts("ERROR: Legal values for log_level are: #{Log::LEVELS}")
        return
      end

      @@options[name] = value

      puts("#{name} => #{value}")
    end
  end

  def Ui.prompt()
    if(@@session.nil?)
      print("dnscat> ")
    else
      if(@@options['prompt'])
        print("dnscat [#{@@session.id}]> ")
      end
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
        Session.list().each_pair do |id, session|
          puts("%5d :: %s" % [id, session.name])
        end
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
        banner("Set <name>=<value> variables")
      end,

      :proc => Proc.new do |opts, optarg|
        if(optarg.length == 0)
          puts("Usage: set <name>=<value>")
        else
          optarg = optarg.join(" ")

          # Split at the '=' sign
          optarg = optarg.split("=", 2)

          # If we don't have a name=value setup, show an error
          if(optarg.length != 2)
            puts("Usage: set <name>=<value>")
          else
            set_option(optarg[0], optarg[1])
          end
        end
      end
    },

    "show" => {
      :parser => Trollop::Parser.new do
        banner("Shows current variables if 'show options' is run. Currently no other functionality")
      end,

      :proc => Proc.new do |opts, optarg|
        if(optarg.count != 1)
          puts("Usage: show options")
        else
          if(optarg[0] == "options")
            @@options.each_pair do |name, value|
              puts("#{name} => #{value}")
            end
          else
            puts("Usage: show options")
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
          @@session.queue_outgoing(line)
        end

        Ui.prompt()
      end

      if(!@@session.nil? && @@session.incoming?)
        Ui.display(@@session.read_incoming)
      end
    end
  end

  def Ui.display(message)
    raise(RuntimeError, "Shouldn't be using this")
  end

  def Ui.error(message)
    $stderr.puts(message)
  end

  def Ui.session_created(id)
  end

  def Ui.session_established(id)
    puts("New session established: #{id}")
    if(!@@options["auto_command"].nil?)
      Session.find(id).queue_outgoing(@@options["auto_command"])
    end

    # If we aren't already in a session, and auto-attach is enabled, attach to it
    if(@@session.nil?() && @@options["auto_attach"])
      Ui.attach(id)
    end
  end

  def Ui.session_data_received(id, data)
    # TODO: Limit the length
    @@data[id] = @@data[id] || ""
    @@data[id] += data

    # If we're currently watching this session, display the data
    if(!@@session.nil? && @@session.id == id)
      if(@@options['prompt'])
        puts()
        puts(data)
      else
        print(data)
      end

      prompt()
    end
  end

  def Ui.session_data_sent(id, data)
  end

  def Ui.session_data_acknowledged(id, data)
    if(!@@session.nil? && @@session.id == id)
      puts()
      puts("[ACK] #{data}")
      prompt()
    end
  end

  def Ui.session_data_queued(id, data)
  end

  def Ui.session_destroyed(id)
    puts("Session terminated: #{id}")

    # If we're connected to the session, close it
    if(!@@session.nil? && @@session.id == id)
      Ui.detach()
    end
  end

  def Ui.dnscat2_state_error(session_id, message)
    $stderr.puts("#{message} :: Session: #{session_id}")
  end

  def Ui.dnscat2_syn_received(session_id, my_seq, their_seq)
  end

  def Ui.dnscat2_msg_bad_seq(expected_seq, received_seq)
  end

  def Ui.dnscat2_msg_bad_ack(expected_ack, received_ack)
    $stderr.puts("WARNING: Impossible ACK received: 0x%04x, current SEQ is 0x%04x" % [received_ack, expected_ack])
  end

  def Ui.dnscat2_msg(incoming, outgoing)
  end

  def Ui.dnscat2_fin()
    $stderr.puts("Received FIN for session #{session.id}; closing session")
  end

  def Ui.dnscat2_recv(packet)
    if(@@options["packet_trace"])
      puts("IN: #{packet}")
    end
  end

  def Ui.dnscat2_send(packet)
    if(@@options["packet_trace"])
      puts("OUT: #{packet}")
    end
  end

  def Ui.log(level, message)
    begin
      # Handle the special case, before a level is set
      if(@@options["log_level"].nil?)
        min = Log::INFO
      else
        min = Log.get_by_name(@@options["log_level"])
      end

      if(level >= min)
        puts("[[#{Log::LEVELS[level]}]] :: #{message}")
      end
    rescue Exception => e
      puts("Error in logging code: #{e}")
      exit
    end
  end

end

# Trap ctrl-z, just like Metasploit
Signal.trap("TSTP") do
  puts()
  Ui.detach
end
