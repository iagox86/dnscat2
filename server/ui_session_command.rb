##
# ui_session_command.rb
# By Ron Bowes
# Created May, 2014
#
# See LICENSE.txt
##

require 'command_packet_stream'
require 'command_packet'

class UiSessionCommand < UiInterface
  attr_reader :session

  ALIASES = {
    "q"    => "quit",
    "exit" => "quit",
  }

  def kill_me()
    puts("Are you sure you want to kill this session? [Y/n]")
    if($stdin.gets[0].downcase != 'n')
      @ui.kill_session(@local_id)
      @ui.attach_session(nil)
    end
  end


  def get_commands()
    return {

      "" => {
        :parser => Trollop::Parser.new do end,
        :proc => Proc.new do |opts| end,
      },

      "quit" => {
        :parser => Trollop::Parser.new do
          banner("Closes and kills this command session")
        end,

        :proc => Proc.new do |opts|
          kill_me()
        end,
      },

      "help" => {
        :parser => Trollop::Parser.new do
          banner("Shows a help menu")
        end,

        :proc => Proc.new do |opts|
          puts("Available session commands:")
          @commands.keys.sort.each do |name|
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

      "ping" => {
        :parser => Trollop::Parser.new do end,
        :proc => Proc.new do |opts|
          packet = CommandPacket.create_ping_request(command_id(), "A"*200)
          @session.queue_outgoing(packet)
          puts("Ping sent!")
        end,
      },

      "shell" => {
        :parser => Trollop::Parser.new do end,
        :proc => Proc.new do |opts|
          packet = CommandPacket.create_shell_request(command_id(), "name")
          @session.queue_outgoing(packet)
          puts("Shell request sent!")
        end,
      },
    }
  end

  def initialize(local_id, session, ui)
    super()

    @local_id = local_id
    @session  = session
    @ui = ui
    @stream = CommandPacketStream.new()
    @command_id = 0x0001

    @commands = get_commands()
  end

  def feed(data)
    @stream.feed(data, false) do |packet|
      puts(packet.to_s)
    end
  end

  def output(str)
    puts()
    puts(str)

    if(attached?())
      print(">> ")
    end
  end

  def error(str)
    puts()
    puts("ERROR: %s" % str)

    if(attached?())
      print(">> ")
    end
  end

  def to_s()
    if(active?())
      idle = Time.now() - @last_seen
      if(idle > 60)
        return "session %5d :: %s :: [idle for over a minute; probably dead]" % [@local_id, @session.name]
      elsif(idle > 5)
        return "session %5d :: %s :: [idle for %d seconds]" % [@local_id, @session.name, idle]
      else
        return "session %5d :: %s" % [@local_id, @session.name]
      end
    else
      return "session %5d :: %s :: [closed]" % [@local_id, @session.name]
    end
  end

  def command_id()
    id = @command_id
    @command_id += 1
    return id
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

    if(@commands[command].nil?)
      puts("Unknown command: #{command}")
    else
      begin
        command = @commands[command]
        opts = command[:parser].parse(args)
        command[:proc].call(opts, args)
      rescue Trollop::CommandlineError => e
        @ui.error("ERROR: #{e}")
      rescue Trollop::HelpNeeded => e
        command[:parser].educate
      rescue Trollop::VersionNeeded => e
        @ui.error("Version needed!")
      end
    end
  end

  def go
    line = Readline::readline("dnscat [command: #{@local_id}]> ", true)

    if(line.nil?)
      kill_me()
      return
    end

    # Otherwise, process the line
    process_line(line)
  end

#      packet = CommandPacket.create_shell_request(command_id(), "name")
#
#      packet = CommandPacket.create_ping_request(command_id(), "A"*200)
#    if(!packet.nil?)
#      @session.queue_outgoing(packet)
#    end

end
