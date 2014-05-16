# ui_command.rb
# By Ron Bowes
# Created July 4, 2013

require 'readline'
require 'ui_new'
require 'ui_interface'

class UiCommandNew < UiInterface
  ALIASES = {
    "q" => "quit"
  }

  def UiCommandNew.do_show_options()
    UiNew.each_option do |name, value|
      puts("#{name} => #{value}")
    end
  end

  def UiCommandNew.do_show_sessions()
    UiNew.each_ui do |local_id, session|
      puts(session.to_s())
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
        UiCommandNew.do_show_sessions()
      end,
    },

    "session" => {
      :parser => Trollop::Parser.new do
        banner("Handle interactions with a particular session (when in interactive mode, use ctrl-z to return to dnscat2)")
        opt :i, "Interact with the chosen session", :type => :integer, :required => false
        opt :l, "List sessions"
      end,

      :proc => Proc.new do |opts|
        if(opts[:l])
          puts("Known sessions:")
          UiCommandNew.do_show_sessions()
        elsif(opts[:i].nil?)
          puts("Known sessions:")
          UiCommandNew.do_show_sessions()
        else
          ui = UiNew.get_by_local_id(opts[:i])
          if(ui.nil?)
            error("Session #{opts[:i]} not found!")
            UiCommandNew.do_show_sessions()
          else
            UiNew.attach_session(ui)
          end
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
          puts()
          UiCommandNew.do_show_options()
        else
          optarg = optarg.join(" ")

          # Split at the '=' sign
          optarg = optarg.split("=", 2)

          # If we don't have a name=value setup, show an error
          if(optarg.length != 2)
            puts("Usage: set <name>=<value>")
          else
            UiNew.set_option(optarg[0], optarg[1])
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
            UiCommandNew.do_show_options()
          else
            puts("Usage: show options")
          end
        end
      end
    },

    "kill" => {
      :parser => Trollop::Parser.new do
        banner("Terminate a session")
      end,

      :proc => Proc.new do |opts, optarg|
        if(optarg.count != 1)
          puts("Usage: kill <session_id>")
        else
          if(UiNew.kill_session(optarg[0].to_i()))
            puts("Session killed")
          else
            puts("Couldn't kill session!")
          end
        end
      end
    }
  }

  def initialize()
  end

  def process_line(line)
    split = line.split(/ /)

    if(split.length > 0)
      command = split.shift
      args = split
    else
      command = ""
      args = ""
    end

    if(ALIASES[command])
      command = ALIASES[command]
    end

    if(COMMANDS[command].nil?)
      puts("Unknown command: #{command}")
    else
      begin
        command = COMMANDS[command]
        opts = command[:parser].parse(args)
        command[:proc].call(opts, args)
      rescue Trollop::CommandlineError => e
        UiNew.error("ERROR: #{e}")
      rescue Trollop::HelpNeeded => e
        command[:parser].educate
      rescue Trollop::VersionNeeded => e
        UiNew.error("Version needed!")
      end
    end
  end

  def go()
    line = Readline.readline("dnscat> ", true)

    # If we hit EOF, terminate
    if(line.nil?)
      puts()
      exit
    end

    # Otherwise, process the line
    process_line(line)
  end

  def attach()
    @is_attached = true
    puts("Welcome to command mode!")
    puts("Type 'help' for a list of commands!")
    puts()

    restore_history()
  end

  def detach()
    @is_attached = false

    save_history()
  end

  def attached?()
    return @is_attached
  end

  def active?()
    return true
  end

  def to_s()
    return "Command window"
  end

  def destroy()
    raise(DnscatException, "Tried to kill the command window!")
  end

  def output(str)
    if(attached?())
      puts()
      puts(str)
      print("[...]#{Readline.line_buffer}")
    end
  end

  def error(str)
    if(attached?())
      $stderr.puts()
      $stderr.puts(str)
      print("[...]#{Readline.line_buffer}")
    end
  end
end
