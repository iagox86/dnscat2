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

require 'libs/command_packet'

class DriverCommand
  def request_id()
    id = @request_id
    @request_id += 1
    return id
  end

  def _send_request(request)
    @handlers[request.get(:request_id)] = {
      :request => request,
      :proc => proc
    }

    out = request.serialize()
    out = [out.length, out].pack("Na*")
    @outgoing += out
  end

  def register_commands()
    @commander.register_command('echo',
      Trollop::Parser.new do
        banner("Print stuff to the terminal, including $variables")
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
        ping = CommandPacket.new({
          :is_request => true,
          :request_id => request_id(),
          :command_id => CommandPacket::COMMAND_PING,
          :data       => (0...opts[:length]).map { ('A'.ord + rand(26)).chr }.join()
        })

        _send_request(ping) do |request, response|
          if(request.get(:data) != response.get(:data))
            @window.puts("The server didn't return the same ping data we sent!")
            @window.puts("Expected: #{request.get(:data)}")
            @window.puts("Received: #{response.get(:data)}")
          else
            @window.puts("Pong!")
          end
        end

        @window.puts("Ping!")
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
        shell = CommandPacket.new({
          :is_request => true,
          :request_id => request_id(),
          :command_id => CommandPacket::COMMAND_SHELL,
          :name       => opts[:name] || "shell"
        })

        _send_request(shell) do |request, response|
          @window.puts("Shell session created: #{response.get(:session_id)}")
        end

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

        if(name == "")
          @window.puts("No command given!")
          @window.puts()
          raise(Trollop::HelpNeeded)
        end

        puts("command = #{command} #{command.class}")
        exec = CommandPacket.new({
          :is_request => true,
          :request_id => request_id(),
          :command_id => CommandPacket::COMMAND_EXEC,
          :command => command,
          :name => name,
        })

        _send_request(exec) do |request, response|
          @window.puts("Executed \"#{request.get(:command)}\": #{response.get(:session_id)}")
        end

        @window.puts("Sent request to execute \"#{command}\"")
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

          download = CommandPacket.new({
            :is_request => true,
            :request_id => request_id(),
            :command_id => CommandPacket::COMMAND_DOWNLOAD,
            :filename => remote_file,
          })

          _send_request(download) do |request, response|
            File.open(local_file, "wb") do |f|
              f.write(response.get(:data))
              @window.puts("Wrote #{response.get(:data).length} bytes from #{request.get(:filename)} to #{local_file}!")
            end
          end

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

          upload = CommandPacket.new({
            :is_request => true,
            :request_id => request_id(),
            :command_id => CommandPacket::COMMAND_UPLOAD,
            :filename => remote_file,
            :data => data,
          })

          _send_request(upload) do |request, response|
            @window.puts("#{data.length} bytes uploaded from #{local_file} to #{remote_file}")
          end

          @window.puts("Attempting to upload #{local_file} to #{remote_file}")
        end
      end
    )

    @commander.register_command("shutdown",
      Trollop::Parser.new do
        banner("Shut down the remote session")
      end,

      Proc.new do |opts, optval|
        _send_request(CommandPacket.create_shutdown_request(request_id())) do |request, response|
          @window.puts("TODO: handle shutdown() response")
        end
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

    @handlers = {}

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

    if(command_packet.get(:is_request))
      @window.puts("ERROR: The client sent us a request! That's not valid (but")
      @window.puts("it may be in the future, so this may be a version mismatch")
      @window.puts("problem)")

      return
    end

    if(@handlers[command_packet.get(:request_id)].nil?)
      @window.puts("Received a response that we have no record of sending:")
      @window.puts("#{command_packet}")
      @window.puts()
      @window.puts("Here are the responses we're waiting for:")
      @handlers.each_pair do |request_id, handler|
        puts("#{request_id}: #{handler[:request]}")
      end

      return
    end

    handler = @handlers.delete(command_packet.get(:request_id))
    handler[:proc].call(handler[:request], command_packet)
  end

  def feed(data)
    @incoming += data
    loop do
      if(@incoming.length < 4)
        break
      end

      # Try to read a length + packet
      length, data = @incoming.unpack("Na*")

      # If there isn't enough data, give up
      if(data.length < length)
        break
      end

      # Otherwise, remove what we have from @data
      length, data, @incoming = @incoming.unpack("Na#{length}a*")
      _handle_incoming(CommandPacket.parse(data, false))
    end

    # Return the queue and clear it out
    result = @outgoing
    @outgoing = ''
    return result
  end
end
