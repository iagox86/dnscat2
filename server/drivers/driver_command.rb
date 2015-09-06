##
# driver_command.rb
# Created August 29, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'shellwords'
require 'pp'

require 'drivers/command_packet'
require 'drivers/command_packet_stream'


class DriverCommand
  def request_id()
    id = @request_id
    @request_id += 1
    return id
  end
  
  def register_commands()
    @commander.register_command('echo',
      Trollop::Parser.new do
        banner("Print stuff to the terminal")
      end,

      Proc.new do |opts, optval|
        @window.puts(optval)
      end
    )

    @commander.register_command("ping",
      Trollop::Parser.new do
        banner("Sends a 'ping' to the remote host to make sure it's still alive)")
        opt :length, "length", :type => :integer, :required => false, :default => 256
      end,
      Proc.new do |opts|
        data = (0...opts[:length]).map { ('A'.ord + rand(26)).chr }.join
        id = request_id()
        @pings[id] = data
        @outgoing += CommandPacket.create_ping_request(id, data)

        @window.puts("Ping request 0x%x sent! (0x%x bytes)" % [id, opts[:length]])
      end,
    )

    @commander.register_command("clear",
      Trollop::Parser.new do
        banner("Clears the display")
      end,
      Proc.new do |opts|
        0.upto(100) do @window.puts() end
      end,
    )

    @commander.register_command("shell",
      Trollop::Parser.new do
        banner("Spawn a shell on the remote host")
        opt :name, "Name", :type => :string, :required => false, :default => nil
      end,

      Proc.new do |opts|
        name = opts[:name] || "executing a shell"

        @outgoing += CommandPacket.create_shell_request(request_id(), name)
        @window.puts("Sent request to execute a shell")
      end,
    )

    @commander.register_command("exec",
      Trollop::Parser.new do
        banner("Execute a program on the remote host")
        opt :command, "Command", :type => :string, :required => false, :default => nil
        opt :name,    "Name",    :type => :string, :required => false, :default => nil
      end,

      Proc.new do |opts, optarg|
        command = opts[:command] || optarg
        name    = opts[:name]    || command

        if(command == "")
          @window.puts("No command given!")
        else
          @outgoing += CommandPacket.create_exec_request(request_id(), name, command)
          @window.puts("Sent request to execute #{opts[:command]}")
        end
      end,
    )

    @commander.register_command("suspend",
      Trollop::Parser.new do
        banner("Go back to the parent session")
      end,

      Proc.new do |opts, optarg|
        @window.deactivate()
      end,
    )

    @commander.register_command("download",
      Trollop::Parser.new do
        banner("Download a file off the remote host. Usage: download <from> [to]")
      end,

      Proc.new do |opts, optval|
        # Get the two files
        remote_file, local_file = Shellwords.shellwords(optval)

        # Sanity check
        if(remote_file.nil? || remote_file == "")
          @window.puts("Usage: download <from> [to]")
        else
          # Make sure we have a local file
          if(local_file.nil? || local_file == "")
            # I only want the filename to prevent accidental traversal
            local_file = File.basename(remote_file)
          end

          id = request_id()
          @downloads[id] = local_file
          @outgoing += CommandPacket.create_download_request(id, remote_file)

          @window.puts("Attempting to download #{remote_file} to #{local_file}")
        end
      end
    )

    @commander.register_command("upload",
      Trollop::Parser.new do
        banner("Upload a file off the remote host. Usage: upload <from> <to>")
      end,

      Proc.new do |opts, optval|
        # Get the two files
        local_file, remote_file = Shellwords.shellwords(optval)

        # Sanity check
        if(local_file.nil? || local_file == "" || remote_file.nil? || remote_file == "")
          @window.puts("Usage: upload <from> <to>")
        else
          data = IO.read(local_file)

          id = request_id()
          @uploads[id] = local_file
          @outgoing += CommandPacket.create_upload_request(id, remote_file, data)

          @window.puts("Attempting to upload #{local_file} to #{remote_file}")
        end
      end
    )

    @commander.register_command("shutdown",
      Trollop::Parser.new do
        banner("Shut down the remote session")
      end,

      Proc.new do |opts, optval|
        outgoing += CommandPacket.create_shutdown_request(request_id())
        @window.puts("Attempting to shut down remote session(s)...")
      end
    )

  end

  def initialize(window)
    @window = window
    @outgoing = ""
    @incoming = ""
    @request_id = 0x0001
    @commander = Commander.new()
    register_commands()

    @pings = {}
    @uploads = {}
    @downloads = {}

    @window.on_input() do |data|
      @commander.feed(data)
    end

    @window.puts("This is a command session!")
    @window.puts()
    @window.puts("That means you can enter a dnscat2 command such as")
    @window.puts("'ping'! For a full list of clients, try 'help'.")
    @window.puts()
  end

  def _handle_incoming(command_packet)
    # TODO: This isn't necessary
    @window.puts("Received: #{command_packet}")

    if(!command_packet.is_response?())
      @window.puts("The client sent us a request packet! We don't accept client requests (although in the future we might, so this may be a version mismatch problem)")
      return
    end

    case command_packet.command_id
    when CommandPacket::COMMAND_PING
      pp @pings
      ping = @pings.delete(command_packet.request_id)

      if(ping.nil?)
        @window.puts("Received a response to a ping we didn't request")
        return
      end

      if(command_packet.data != ping)
        @window.puts("The server didn't return the same ping data we sent!")
        @window.puts("Expected: #{ping.data}")
        @window.puts("Received: #{command_packet.data}")
        return
      end

      @window.puts("Ping response successfully received!")
    when CommandPacket::COMMAND_SHELL
      @window.puts("Shell session created: #{command_packet.session_id}")
    when CommandPacket::COMMAND_EXEC
      @window.puts("Command executed in session: #{command_packet.session_id}")
    when CommandPacket::COMMAND_DONWLOAD
      # TODO
    when CommandPacket::COMMAND_UPLOAD
    when CommandPacket::COMMAND_SHUTDOWN
    when CommandPacket::COMMAND_ERROR
    end
  end

  def feed(data)
    @incoming += data

    loop do
      if(@incoming.length < 4)
        break
      end

      length, data = @incoming.unpack("Na*")
      if(data.length < length)
        return ''
      end

      _, @incoming = data.unpack("a#{length}a*")

      _handle_incoming(CommandPacket.new(data, false))
    end

    result = @outgoing
    @outgoing = ''
    return result
  end
end
