##
# driver_command_tunnels.rb
# Created November 27, 2015
# By Ron Bowes
#
# See: LICENSE.md
##

require 'libs/command_helpers'
require 'libs/socketer'

class ViaSocket
  attr_reader :tunnel_id

  @@via_sockets = {}

  def initialize(driver, host, port, callbacks)
    @tunnel_id = nil
    @session   = nil
    @closed    = false
    @driver    = driver

    # Ask the client to make a connection for us
    packet = CommandPacket.new({
      :is_request => true,
      :request_id => driver.request_id(),
      :command_id => CommandPacket::TUNNEL_CONNECT,
      :options    => 0,
      :host       => host,
      :port       => port,
    })

    driver._send_request(packet, Proc.new() do |request, response|
      # Handle an error response
      if(response.get(:command_id) == CommandPacket::COMMAND_ERROR)
        if(callbacks[:on_error])
          callbacks[:on_error].call(nil, "Connect failed: #{response.get(:reason)}", e)
        end

        next
      end

      # Create a socket pair so we can use this with Socketer
      # We talk to @v_socket a lot, so we keep a handle to it, but once
      # the Socketer::Manager is started we no longer need s_socket
      @v_socket, s_socket = UNIXSocket.pair()

      # Get the tunnel_id
      @tunnel_id = response.get(:tunnel_id)

      # If the response was good, then we can create a Socketer Manager and hook up to it!
      @smanager = Socketer::Manager.new(s_socket, callbacks)

      # Save ourselves in the list of instances
      @@via_sockets[@tunnel_id] = self

      # Start a receive thread for the socket
      @thread = Thread.new() do
        begin
          loop do
            data = @v_socket.recv(Socketer::BUFFER)

            driver._send_request(CommandPacket.new({
              :is_request => true,
              :request_id => driver.request_id(),
              :command_id => CommandPacket::TUNNEL_DATA,
              :tunnel_id  => @tunnel_id,
              :data       => data,
            }), nil)
          end
        rescue StandardError => e
          puts("Error in ViaSocket receive thread: #{e}")
        end
      end

      if(callbacks[:on_via])
        callbacks[:on_via].call(@smanager, self)
      end

      # We're only ready AFTER we are prepared to receive data - this may do a callback instantly
      @smanager.ready!()
    end)
  end

  def wait_for_smanager()
    while(@smanager.nil?)
      # TODO: Get rid of this
      puts("Waiting for the tunnel to connect...")
      sleep(0.1)
    end
  end

  def write(data)
    wait_for_smanager()

    @smanager.write(data)
  end

  def close(send_close = false)
    if(@closed)
      puts("This via_socket is already closed!")
      return
    end
    @closed = true

    puts("Closing #{@v_socket}")
    @v_socket.close()
    @smanager.close()

    if(send_close)
      @driver._send_request(CommandPacket.new({
        :is_request => true,
        :request_id => @driver.request_id(),
        :command_id => CommandPacket::TUNNEL_CLOSE,
        :tunnel_id  => @tunnel_id,
        :reason     => "Socket closed",
      }), nil)
    end

    # Close the thread last in case we're in it
    @thread.exit()
  end

  def _sneak_in_data(data)
    @v_socket.write(data)
  end

  def ViaSocket.get(driver, tunnel_id)
    via_socket = @@via_sockets[tunnel_id]
    if(via_socket.nil?)
      puts("ERROR: Couldn't find the socket for tunnel #{tunnel_id}")

      driver._send_request(CommandPacket.new({
        :is_request => true,
        :request_id => driver.request_id(),
        :command_id => CommandPacket::TUNNEL_CLOSE,
        :tunnel_id  => tunnel_id,
        :reason     => "Unknown tunnel: %d" % tunnel_id
      }), nil)

      return nil
    end

    return via_socket
  end

  def ViaSocket.handle_packet(driver, packet)
    tunnel_id = packet.get(:tunnel_id)
    via_socket = ViaSocket.get(driver, tunnel_id) # TODO: Check for errors

    case packet.get(:command_id)
    when CommandPacket::TUNNEL_DATA
      puts("Received TUNNEL_DATA")
      via_socket._sneak_in_data(packet.get(:data))

    when CommandPacket::TUNNEL_CLOSE
      puts("Recieved TUNNEL_CLOSE")
      puts("Closing via_socket in ViaSocket.handle_packet()")
      via_socket.close()
    else
      raise(DnscatException, "Unknown command sent by the server: #{packet}")
    end
  end
end

module DriverCommandTunnels
  def _parse_host_ports(str)
    local, remote = str.split(/ /)

    if(remote.nil?)
      @window.puts("Bad argument! Expected: 'listen [<lhost>:]<lport> <rhost>:<rport>'")
      @window.puts()
      raise(Trollop::HelpNeeded)
    end

    # Split the local port at the :, if there is one
    if(local.include?(":"))
      local_host, local_port = local.split(/:/)
    else
      local_host = '0.0.0.0'
      local_port = local
    end
    local_port = local_port.to_i()

    if(local_port <= 0 || local_port > 65535)
      @window.puts("Bad argument! lport must be a valid port (between 0 and 65536)")
      @window.puts()
      raise(Trollop::HelpNeeded)
    end

    remote_host, remote_port = remote.split(/:/)
    if(remote_host == '' || remote_port == '' || remote_port.nil?)
      @window.puts("rhost or rport missing!")
      @window.puts()
      raise(Trollop::HelpNeeded)
    end
    remote_port = remote_port.to_i()

    if(remote_port <= 0 || remote_port > 65535)
      @window.puts("Bad argument! rport must be a valid port (between 0 and 65536)")
      @window.puts()
      raise(Trollop::HelpNeeded)
    end

    return local_host, local_port, remote_host, remote_port
  end

  def _register_commands_tunnels()
    @sessions = {}
    @via_sockets = {}

    @commander.register_command('wget',
      Trollop::Parser.new do
        banner("Perform an HTTP download via an established tunnel")
      end,

      Proc.new do |opts, optarg|
        host = 'localhost'
        port = 4444

        ViaSocket.new(self, host, port, {
          :on_ready => Proc.new() do |manager|
            @window.puts("Connection successful: #{host}:#{port}")
            manager.write("GET / HTTP/1.0\r\n\r\n")
          end,
          :on_error => Proc.new() do |manager, msg, e|
            @window.puts("ERROR: #{msg} #{e}")
          end,
          :on_data => Proc.new() do |manager, data|
            @window.puts("Data received: #{data}")
          end,
        })
      end
    )

#    @commander.register_command('tunnels',
#      Trollop::Parser.new do
#        banner("Lists all current listeners")
#      end,
#
#      Proc.new do |opts, optarg|
#        @tunnels.each do |tunnel|
#          @window.puts(tunnel.to_s)
#        end
#      end
#    )

    @commander.register_command('listen',
      Trollop::Parser.new do
        banner("Listens on a local port and sends the connection out the other side (like ssh -L). Usage: listen [<lhost>:]<lport> <rhost>:<rport>")
      end,

      Proc.new do |opts, optarg|
        lhost, lport, rhost, rport = _parse_host_ports(optarg)
        @window.puts("Listening on #{lhost}:#{lport}, sending connections to #{rhost}:#{rport}")

        begin
          Socketer::Listener.new(lhost, lport, Proc.new() do |s|
            local_socket = nil
            via_socket = nil

            local_socket = Socketer::Manager.new(s, {
              :on_ready => Proc.new() do |manager|
                puts("local_socket is ready!")
              end,
              :on_data => Proc.new() do |manager, data|
                puts("local_socket got data!")
                via_socket.write(data)
              end,
              :on_error => Proc.new() do |manager, msg, e|
                puts("local_socket#on_error #{manager} #{msg} #{e}")
                via_socket.close(true)
              end,
              :on_close => Proc.new() do |manager|
                puts("local_socket#on_close #{manager}")
                via_socket.close(true)
              end,
            })

            via_socket = ViaSocket.new(self, rhost, rport, {
              :on_ready => Proc.new() do |manager|
                puts("via_socket is ready!")
                local_socket.ready!()
              end,
              :on_data => Proc.new() do |manager, data|
                puts("via_socket got data!")
                local_socket.write(data)
              end,
              :on_error => Proc.new() do |manager, msg, e|
                puts("via_socket#on_error #{manager} #{msg} #{e}")
                local_socket.close()
              end,
              :on_close => Proc.new() do |manager|
                puts("via_socket#on_close #{manager}")
                local_socket.close()
              end,
            })

          end)
        rescue Errno::EACCES => e
          @window.puts("Sorry, couldn't listen on that port: #{e}")
        rescue Errno::EADDRINUSE => e
          @window.puts("Sorry, that address:port is already in use: #{e}")
          # TODO: Better error msg
        rescue Exception => e
          @window.puts("An exception occurred: #{e}")
        end
      end
    )
  end

  def tunnel_data_incoming(packet)
    ViaSocket.handle_packet(self, packet)
  end

  def tunnels_stop()
#    if(@tunnels.length > 0)
#      @window.puts("Stopping active tunnels...")
#      @tunnels.each do |t|
#        t.kill()
#      end
#    end
  end
end
