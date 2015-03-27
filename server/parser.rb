# ui_command.rb
# By Ron Bowes
# Created July 4, 2013
#
# See LICENSE.md

require 'shellwords'

module Parser
  def initialize_parser(prompt, settings)
    @prompt = prompt
    @commands = {}
    @aliases = {}
    @settings = settings

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
    # Do find/replace for settings
    @settings.each_pair do |name, value|
      line = line.gsub(/\$#{name}/, value.to_s)
    end

    # If the line starts with a '!', just pass it to a shell
    if(line[0] == '!')
      system(line[1..-1])
      return
    end

    begin
      args = Shellwords.shellwords(line)
    rescue Exception => e
      error("Parse failed: #{e}")
      return
    end

    if(args.length > 0)
      command = args.shift
    else
      command = ""
      args = []
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
    begin
      process_line(line)
#    rescue SystemExit
#      exit()
    rescue SystemExit
      exit
    rescue Exception => e
      if(e.to_s() =~ /wakeup/i)
        # do nothing
      else
        error("There was an error processing the line: #{e}")
        error("If you think it was my fault, please submit a bug report with the following stacktrace:")
        error("")
        error(e.backtrace)
      end
    end
  end
end
