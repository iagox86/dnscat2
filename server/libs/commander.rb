# commander.rb
# By Ron Bowes
# Created August 29, 2015
#
# See LICENSE.md

class Commander
  # TODO: Handle settings (replacing $var with a name)
  # TODO: Handle shell escapes ('!command')
  def initialize(parent = nil)
    @parent = parent
    @commands = {}
    @aliases = {}
  end

  def register_command(name, parser, func)
    @commands[name] = {
      :parser => parser,
      :func   => func,
    }
  end

  def register_alias(name, points_to)
    @aliases[name] = points_to
  end

  def _resolve_alias(command)
    while(!@aliases[command].nil?)
      command = @aliases[command]
    end

    return command
  end

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

  def help(stream)
    stream.puts()
    stream.puts("Here is a list of commands (use -h on any of them for additional help):")
    @commands.keys.sort.each do |command|
      stream.puts("* #{command}")
    end
  end

  # Can throw an ArgumentError for various reasons
  def feed(data)
    args = Shellwords.shellwords(data)

    if(args.length == 0)
      return
    end
    command = _resolve_alias(args.shift())

    if(@commands[command].nil?)
      # TODO: Somehow pass to parent (probably in session.rb)
      if(!@parent.nil?)
        return @parent.feed(data)
      end

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
