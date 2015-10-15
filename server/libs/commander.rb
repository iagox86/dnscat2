##
# commander.rb
# By Ron Bowes
# Created August 29, 2015
#
# See LICENSE.md
#
# This is a class designed for registering commandline-style commands that
# are parsed and passed to handlers.
#
# After instantiating this class, register_command() is used to register
# new commands. register_alias() can also be used to create command aliases.
#
# Later, when the user types something that should be parsed as a command, the
# feed() function is called with the string the user passed. It attempts to
# parse it as a commandline-like string and call the appropriate function.
#
# This class uses the shellwords and trollop libraries to do much of the heavy
# lifting.
##

require 'shellwords'
require 'trollop'

class Commander
  def initialize()
    @commands = {}
    @aliases = {}
  end

  # Register a new command. The 'name' is the name the user types to activate
  # the command. The parser is a Trollop::Parser instance, have a look at
  # controller_commands.rb or driver_command_commands.rb to see how that's used.
  # The func is a Proc that's called with two variables: opts, which is the
  # command-line arguments parsed as per the parser; and optarg, which is
  # everything that isn't parsed.
  def register_command(name, parser, func)
    @commands[name] = {
      :parser => parser,
      :func   => func,
    }
  end

  # Register an alias; if the user types 'name', it calls the handler for
  # 'points_to'. This is recursive.
  def register_alias(name, points_to)
    @aliases[name] = points_to
  end

  def _resolve_alias(command)
    while(!@aliases[command].nil?)
      command = @aliases[command]
    end

    return command
  end

  # Treating data as either a command or a full command string, this gets
  # information about the command the user tried to use, and prints help
  # to stream (stream.puts() and stream.printf() are required). 
  #
  # The dependence on passing in a stream is sub-optimal, but it's the only
  # way that Trollop works.
  def educate(data, stream)
    begin
      args = Shellwords.shellwords(data)
    rescue ArgumentError
      return
    end

    if(args.length == 0)
      return
    end

    command = args.shift()
    if(command.nil?)
      return
    end

    command = _resolve_alias(command)

    command = @commands[command]
    if(command.nil?)
      return
    end

    command[:parser].educate(stream)
  end

  # Print all commands to stream.
  def help(stream)
    stream.puts()
    stream.puts("Here is a list of commands (use -h on any of them for additional help):")
    @commands.keys.sort.each do |command|
      stream.puts("* #{command}")
    end
  end

  # Try to parse a line typed in by the user as a command, calling the
  # appropriate handler function, if the user typed a bad command.
  #
  # This will throw an ArgumentError for various reasons, including
  # unknown commands, badly quoted strings, and bad arguments.
  def feed(data)
    if(data[0] == '!')
      system(data[1..-1])
      return
    end
    args = Shellwords.shellwords(data)

    if(args.length == 0)
      return
    end
    command = _resolve_alias(args.shift())

    if(@commands[command].nil?)
      raise(ArgumentError, "Unknown command: #{command}")
    end

    begin
      command = @commands[command]
      command[:parser].stop_on("--")
      opts = command[:parser].parse(args)
      optval = ""
      optarr = command[:parser].leftovers
      if(!optarr.nil?())
        if(optarr[0] == "--")
          optarr.shift()
        end
        optval = optarr.join(" ")
      end
      command[:func].call(opts, optval)
    rescue Trollop::CommandlineError => e
      raise(ArgumentError, e.message)
    rescue Trollop::HelpNeeded => e
      raise(ArgumentError, "The user requested help")
    end
  end
end
