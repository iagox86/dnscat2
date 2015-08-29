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

  def feed(data)
    begin
      args = Shellwords.shellwords(data)
    rescue ArgumentError => e
      # TODO: How do I handle an ArgumentError right?
      return false
    end

    if(args.length == 0)
      return
    end

    command = args.shift()

    while(!@aliases[command].nil?)
      command = @aliases[command]
    end

    if(@commands[command].nil?)
      # TODO: Somehow pass to parent (probably in session.rb)
      if(!@parent.nil?)
        return @parent.feed(data)
      end

      puts("Unknown command")
      return false
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
      puts("ERROR: #{e}")
      return false
    rescue Trollop::HelpNeeded => e
      puts("Help needed")
      #command[:parser].educate
      return false
    end

    return true
  end
end
