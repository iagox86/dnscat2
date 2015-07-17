##
# ui_session_command.rb
# By Ron Bowes
# Created May, 2014
#
# See LICENSE.md
##

require 'command_packet_stream'
require 'command_packet'
require 'parser'
require 'ui_handler'
require 'ui_interface_with_id'

class UiSessionCommand < UiInterfaceWithId
  attr_reader :session
  attr_reader :id

  include Parser
  include UiHandler

  def kill_me()
    puts()
    puts("Are you sure you want to kill this session? [Y/n]")
    if($stdin.gets[0].downcase == 'n')
      puts("You might want to use ^z or the 'suspend' command")
    else
      @ui.kill_session(@id)
      @ui.detach_session()
    end
  end

  def register_commands()
    register_alias('q',       'quit')
    register_alias('exit',    'quit')
    register_alias('run',     'exec')
    register_alias('execute', 'exec')
    register_alias('up',      'suspend')
    register_alias('back',    'suspend')
    register_alias('pause',   'suspend')

    register_command("quit",
      Trollop::Parser.new do
        banner("Closes and kills this command session")
      end,

      Proc.new do |opts|
        kill_me()
      end,
    )

    register_command("help",
      Trollop::Parser.new do
        banner("Shows a help menu")
      end,

      Proc.new do |opts|
        puts()
        puts("Available session commands:")
        @commands.keys.sort.each do |name|
          # Don't display the empty command
          if(name != "")
            puts("- #{name}")
          end
        end

        puts("For more information, --help can be passed to any command")
      end,
    )

    register_command("clear",
      Trollop::Parser.new do
        banner("Clears the display")
      end,
      Proc.new do |opts|
        0.upto(100) do puts() end
      end,
    )

    register_command("ping",
      Trollop::Parser.new do
        banner("Sends a 'ping' to the remote host to make sure it's still alive)")
        opt :length, "length", :type => :integer, :required => false, :default => 256
      end,
      Proc.new do |opts|
        data = "A" * opts[:length]
        id = request_id()
        @pings[id] = data
        packet = CommandPacket.create_ping_request(id, data)
        @session.queue_outgoing(packet)
        puts("Ping request 0x%x sent! (0x%x bytes)" % [id, opts[:length]])
      end,
    )

    register_command("shell",
      Trollop::Parser.new do
        banner("Spawn a shell on the remote host")
        opt :name, "Name", :type => :string, :required => false, :default => nil
      end,

      Proc.new do |opts|
        name = opts[:name] || "executing a shell"

        packet = CommandPacket.create_shell_request(request_id(), name)
        @session.queue_outgoing(packet)
        puts("Sent request to execute a shell")
      end,
    )

    register_command("exec",
      Trollop::Parser.new do
        banner("Execute a program on the remote host")
        opt :command, "Command", :type => :string, :required => false, :default => nil
        opt :name,    "Name",    :type => :string, :required => false, :default => nil
      end,

      Proc.new do |opts, optarg|
        command = opts[:command] || optarg
        name    = opts[:name]    || ("executing %s" % command)

        if(command == "")
          puts("No command given!")
        else
          packet = CommandPacket.create_exec_request(request_id(), name, command)
          @session.queue_outgoing(packet)
          puts("Sent request to execute #{opts[:command]}")
        end
      end,
    )

    register_command("suspend",
      Trollop::Parser.new do
        banner("Go back to the main menu")
      end,

      Proc.new do |opts, optarg|
        @ui.detach_session()
      end,
    )

    register_command('sessions',
      Trollop::Parser.new do
        banner("Lists the current active sessions managed by this session")
        opt :all, "Show dead sessions", :type => :boolean, :required => false
      end,

      Proc.new do |opts, optval|
        puts("Sessions:")
        puts()
        @ui.get_command().display_uis(opts[:all], self, self)
      end,
    )

    register_command("session",
      Trollop::Parser.new do
        banner("Interact with a session")
        opt :i, "Interact with the chosen session", :type => :integer, :required => false
      end,

      Proc.new do |opts, optval|
        if(opts[:i].nil?)
          puts("Known sessions:")
          @ui.get_command().display_uis(false, self, self)
        else
          ui = @ui.get_by_id(opts[:i])
          if(ui.nil?)
            error("Session #{opts[:i]} not found!")
            @ui.get_command().display_uis(false, self, self)
          else
            @ui.attach_session(ui)
          end
        end
      end
    )

    register_command("download",
      Trollop::Parser.new do
        banner("Download a file off the remote host. Usage: download <from> [to]")
      end,

      Proc.new do |opts, optval|
        # Get the two files
        remote_file, local_file = Shellwords.shellwords(optval)

        # Sanity check
        if(remote_file.nil? || remote_file == "")
          puts("Usage: download <from> [to]")
        else
          # Make sure we have a local file
          if(local_file.nil? || local_file == "")
            # I only want the filename to prevent accidental traversal
            local_file = File.basename(remote_file)
          end

          id = request_id()
          @downloads[id] = local_file

          packet = CommandPacket.create_download_request(id, remote_file)
          @session.queue_outgoing(packet)
          puts("Attempting to download #{remote_file} to #{local_file}")
        end
      end
    )

    register_command("upload",
      Trollop::Parser.new do
        banner("Upload a file off the remote host. Usage: upload <from> <to>")
      end,

      Proc.new do |opts, optval|
        # Get the two files
        local_file, remote_file = Shellwords.shellwords(optval)

        # Sanity check
        if(local_file.nil? || local_file == "" || remote_file.nil? || remote_file == "")
          puts("Usage: upload <from> <to>")
        else
          data = IO.read(local_file)

          packet = CommandPacket.create_upload_request(request_id(), remote_file, data)
          @session.queue_outgoing(packet)
          puts("Attempting to upload #{local_file} to #{remote_file}")
        end
      end
    )

    register_command("shutdown",
      Trollop::Parser.new do
        banner("Shut down the remote session and any child sessions")
      end,

      Proc.new do |opts, optval|
        packet = CommandPacket.create_shutdown_request(request_id())
        @session.queue_outgoing(packet)
        puts("Attempting to shut down remote session(s)...")
      end
    )
  end

  def initialize(id, session, ui)
    super(id)

    initialize_parser("dnscat [command: #{id}]> ", ui.settings)
    initialize_ui_handler()

    @id = id
    @session = session
    @ui = ui
    @stream = CommandPacketStream.new()
    @request_id = 0x0001
    @pings = {}
    @downloads = {}

    register_commands()

    # Process automatic commands
    auto = ui.settings.get("auto_command")
    if(auto)
      auto.split(/;/).each() do |line|
        process_line(line)
      end
    end

    puts("Welcome to a command session! Use 'help' for a list of commands or ^z for the main menu")
  end

  def handle_ping_response(packet)
    data = packet.data
    expected = @pings[packet.request_id]
    if(expected.nil?)
      puts("Unexpected ping response received")
    elsif(data == expected)
      puts("Ping response 0x%x received!" % packet.request_id)
      @pings.delete(packet.request_id)
    else
      puts("Ping response 0x%x was invalid!" % packet.request_id)
    end
  end

  # Handles any response that contains a new session id (shell and exec, initially)
  def handle_session_response(packet)
    # Globally create the session
    add_pending(packet.session_id)
  end

  def handle_shell_response(packet)
    handle_session_response(packet)
  end

  def handle_exec_response(packet)
    handle_session_response(packet)
  end

  def handle_download_response(packet)
    file = @downloads[packet.request_id]

    if(file.nil?)
      error("Got a file response for a command we didn't send?")
    else
      if(file == "-")
        puts(packet.data)
        puts()
        puts("Received 0x%x bytes!" % [packet.data.length, file])
      else
        File.open(file, "wb") do |f|
          f.write(packet.data)
        end
        puts("Received 0x%x bytes into %s!" % [packet.data.length, file])
      end
    end
#$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9
  end

  def handle_upload_response(packet)
    puts("File uploaded!")
  end

  def handle_error_response(packet)
    Log.ERROR(@id, "Client responded with error #{packet.status}: #{packet.reason}")
  end

  def feed(data)
    @stream.feed(data, false) do |packet|
      if(packet.is_response?())
        if(packet.command_id == CommandPacket::COMMAND_PING)
          handle_ping_response(packet)
        elsif(packet.command_id == CommandPacket::COMMAND_SHELL)
          handle_shell_response(packet)
        elsif(packet.command_id == CommandPacket::COMMAND_EXEC)
          handle_exec_response(packet)
        elsif(packet.command_id == CommandPacket::COMMAND_DOWNLOAD)
          handle_download_response(packet)
        elsif(packet.command_id == CommandPacket::COMMAND_UPLOAD)
          handle_upload_response(packet)
        elsif(packet.command_id == CommandPacket::COMMAND_ERROR)
          handle_error_response(packet)
        else
          output("Didn't know how to handle command packet: %s" % packet.to_s)
        end
      else
        raise(DnscatException, "We got a command request packet somehow")
      end
    end
  end

  def output(str)
#    puts()
    puts(str)

#    if(attached?())
#      print(">> ")
#    end
  end

  def error(str)
    puts("%s" % str)
  end

  def to_s()
    if(active?())
      idle = Time.now() - @last_seen
      if(idle > 120)
        return "session %d %s:: %s :: [idle for over two minutes; probably dead]" % [@id, activity_indicator(), @session.name]
      elsif(idle > 5)
        return "session %d %s:: %s :: [idle for %d seconds]" % [@id, activity_indicator(), @session.name, idle]
      else
        return "session %d %s:: %s" % [@id, activity_indicator(), @session.name]
      end
    else
      return "session %d %s:: %s :: [closed]" % [@id, activity_indicator(), @session.name]
    end
  end

  def request_id()
    id = @request_id
    @request_id += 1
    return id
  end
end
