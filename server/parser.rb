# ui_command.rb
# By Ron Bowes
# Created July 4, 2013

require 'shellwords'

module Parser
  def initialize_parser(prompt)
    @prompt = prompt
    @commands = {}
    @aliases = {}

    register_command("", Trollop::Parser.new do end, Proc.new do end)
  end

  def register_command(command, parser, func)
    @commands[command] = {
      :parser => parser,
      :proc   => func,
    }
  end

  def register_alias(name, func)
    @aliases[name] = func
  end

  def process_line(line)
    split = line.split(/ /, 2)

    if(split.length > 0)
      command = split.shift
      if(split.length > 0)
        args = Shellwords.shellwords(split.shift)
      else
        args = ""
      end
    else
      command = ""
      args = ""
    end

    if(@aliases[command])
      command = @aliases[command]
    end

    if(@commands[command].nil?)
      puts("Unknown command: #{command}")
    else
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
        command[:proc].call(opts, optval)
      rescue Trollop::CommandlineError => e
        @ui.error("ERROR: #{e}")
      rescue Trollop::HelpNeeded => e
        command[:parser].educate
      end
    end
  end

  def go()
    line = Readline.readline(@prompt, true)

    # If we hit EOF, terminate
    if(line.nil?)
      puts()
      exit
    end

    # Otherwise, process the line
    process_line(line)
  end
end
