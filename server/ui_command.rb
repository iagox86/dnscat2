# ui_command.rb
# By Ron Bowes
# Created July 4, 2013

require "readline"
require 'ui'

class UiCommand
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
          Ui.attach_session(opts[:id])
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
            Ui.set_option(optarg[0], optarg[1])
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
            Ui.each_option do |name, value|
              puts("#{name} => #{value}")
            end
          else
            puts("Usage: show options")
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

  def go()
    line = Readline.readline("dnscat> ", true)
    process_line(line)
  end
end
