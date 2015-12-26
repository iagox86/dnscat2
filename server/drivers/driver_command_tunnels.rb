##
# driver_command_tunnels.rb
# Created November 27, 2015
# By Ron Bowes
#
# See: LICENSE.md
##

require 'libs/command_helpers'
require 'libs/socketer'

module DriverCommandTunnels
  class ViaSocket
    @@instances = {}

    def initialize(driver, name, thread, tunnel_id, v_socket, s_socket, callbacks)
      @driver    = driver
      @name      = name
      @thread    = thread
      @tunnel_id = tunnel_id
      @v_socket  = v_socket
      @s_socket  = s_socket

      # If the response was good, then we can create a Socketer and hook up to it!
      @session = Socketer::Session.new(@s_socket, "#{name} via tunnel #{tunnel_id}", callbacks)

      # Get ready to receive data immediately
      @@instances[tunnel_id] = self

      # We're only ready AFTER we are prepared to receive data - this may do a callback instantly
      @session.ready!()
    end

    def ViaSocket.connect(driver, host, port, callbacks)
      packet = CommandPacket.new({
        :is_request => true,
        :request_id => driver.request_id(),
        :command_id => CommandPacket::TUNNEL_CONNECT,
        :options    => 0,
        :host       => host,
        :port       => port,
      })

      # Create a socket pair so we can use this with Socketer
      v_socket, s_socket = UNIXSocket.pair()

      driver._send_request(packet, Proc.new() do |request, response|
        # Handle an error response
        if(response.get(:command_id) == CommandPacket::COMMAND_ERROR)
          if(callbacks[:on_error])
            callbacks[:on_error].call(self, "Connect failed: #{response.get(:reason)}", e)
          end

          v_socket.close()
          s_socket.close()

          next
        end

        # Get the tunnel_id
        tunnel_id = response.get(:tunnel_id)

        # Start a receive thread for the socket
        thread = Thread.new() do
          loop do
            data = v_socket.recv(Socketer::BUFFER)

            packet = CommandPacket.new({
              :is_request => true,
              :request_id => driver.request_id(),
              :command_id => CommandPacket::TUNNEL_DATA,
              :tunnel_id  => tunnel_id,
              :data => data,
            })

            driver._send_request(packet, nil)
          end
        end

        # Create the socket; we don't have to do anything with it, it looks after itself!
        ViaSocket.new(driver, "#{host}:#{port}", thread, tunnel_id, v_socket, s_socket, callbacks)
      end)
    end

    def write(data)
      @v_socket.write(data)
    end

    def close()
      @v_socket.close()
      @thread.exit()
    end

    def ViaSocket.handle(packet)
      instance = @@instances[packet.get(:tunnel_id)]
      if(instance.nil?)
        return CommandPacket.new({
          :is_request => true,
          :request_id => request_id(),
          :command_id => CommandPacket::TUNNEL_CLOSE,
          :tunnel_id  => tunnel_id,
          :reason     => "Unknown tunnel: %d" % tunnel_id
        })
      end

      case packet.get(:command_id)
      when CommandPacket::TUNNEL_DATA
        instance.write(packet.get(:data))

      when CommandPacket::TUNNEL_CLOSE
        instance.close()

      else
        raise(DnscatException, "Unknown command sent by the server: #{packet}")
      end

      return nil
    end
  end

  def _register_commands_tunnels()
    @tunnels_by_session = {}
    @sessions_by_tunnel = {}
    @tunnels = []

    @commander.register_command('wget',
      Trollop::Parser.new do
        banner("Perform an HTTP download via an established tunnel")
      end,

      Proc.new do |opts, optarg|
        ViaSocket.connect(self, 'localhost', 4444, {
          :on_ready => Proc.new() do |session, name|
            @window.puts("Connection successful: #{name}")
            session.send("GET / HTTP/1.0\r\n\r\n")
          end,
          :on_error => Proc.new() do |session, msg, e|
            @window.puts("ERROR: #{msg} #{e}")
          end,
          :on_data => Proc.new() do |session, data|
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
#
#    @commander.register_command('listen',
#      Trollop::Parser.new do
#        banner("Listens on a local port and sends the connection out the other side (like ssh -L). Usage: listen [<lhost>:]<lport> <rhost>:<rport>")
#      end,
#
#      Proc.new do |opts, optarg|
#        local, remote = optarg.split(/ /)
#
#        if(remote.nil?)
#          @window.puts("Bad argument! Expected: 'listen [<lhost>:]<lport> <rhost>:<rport>'")
#          @window.puts()
#          raise(Trollop::HelpNeeded)
#        end
#
#        # Split the local port at the :, if there is one
#        if(local.include?(":"))
#          local_host, local_port = local.split(/:/)
#        else
#          local_host = '0.0.0.0'
#          local_port = local
#        end
#        local_port = local_port.to_i()
#
#        if(local_port <= 0 || local_port > 65535)
#          @window.puts("Bad argument! lport must be a valid port (between 0 and 65536)")
#          @window.puts()
#          raise(Trollop::HelpNeeded)
#        end
#
#        remote_host, remote_port = remote.split(/:/)
#        if(remote_host == '' || remote_port == '' || remote_port.nil?)
#          @window.puts("rhost or rport missing!")
#          @window.puts()
#          raise(Trollop::HelpNeeded)
#        end
#        remote_port = remote_port.to_i()
#
#        if(remote_port <= 0 || remote_port > 65535)
#          @window.puts("Bad argument! rport must be a valid port (between 0 and 65536)")
#          @window.puts()
#          raise(Trollop::HelpNeeded)
#        end
#
#        @window.puts("Listening on #{local_host}:#{local_port}, sending connections to #{remote_host}:#{remote_port}")
#
#        begin
#          @tunnels << Socketer.listen(local_host, local_port, {
#            :on_ready => Proc.new() do |session, name|
#              @window.puts("Connection from #{name}; forwarding to #{remote_host}:#{remote_port}...")
#
#              packet = CommandPacket.new({
#                :is_request => true,
#                :request_id => request_id(),
#                :command_id => CommandPacket::TUNNEL_CONNECT,
#                :options    => 0,
#                :host       => remote_host,
#                :port       => remote_port,
#              })
#
#              _send_request(packet, Proc.new() do |request, response|
#                if(response.get(:command_id) == CommandPacket::COMMAND_ERROR)
#                  @window.puts("Tunnel error: #{response.get(:reason)}")
#                  session.stop!()
#                else
#                  @window.puts("[Tunnel %d] connection successful!" % response.get(:tunnel_id))
#                  @tunnels_by_session[session] = response.get(:tunnel_id)
#                  @sessions_by_tunnel[response.get(:tunnel_id)] = session
#
#                  # Tell the tunnel that we're ready to receive data
#                  session.ready!()
#                end
#              end)
#            end,
#
#            :on_data => Proc.new() do |session, data|
#              tunnel_id = @tunnels_by_session[session]
#
#              packet = CommandPacket.new({
#                :is_request => true,
#                :request_id => request_id(),
#                :command_id => CommandPacket::TUNNEL_DATA,
#                :tunnel_id => tunnel_id,
#                :data => data,
#              })
#
#              _send_request(packet, nil)
#            end,
#
#            :on_error => Proc.new() do |session, msg, e|
#              # Delete the tunnel
#              tunnel_id = @tunnels_by_session.delete(session)
#              @window.puts("[Tunnel %d] error: %s" % [tunnel_id, msg])
#
#              @sessions_by_tunnel.delete(tunnel_id)
#
#              packet = CommandPacket.new({
#                :is_request => true,
#                :request_id => request_id(),
#                :command_id => CommandPacket::TUNNEL_CLOSE,
#                :tunnel_id  => tunnel_id,
#                :reason     => "Error during the connection: %s" % msg,
#              })
#
#              _send_request(packet, nil)
#            end
#          })
#        rescue Errno::EACCES => e
#          @window.puts("Sorry, couldn't listen on that port: #{e}")
#        rescue Errno::EADDRINUSE => e
#          @window.puts("Sorry, that address:port is already in use: #{e}")
#          @window.puts()
#          @window.puts("If you kill a session from the root window with the 'kill'")
#          @window.puts("command, it will free the socket. You can get a list of which")
#          @window.puts("sockets are being used with the 'tunnels' command!")
#          @window.puts()
#          @window.puts("I realize this is super awkward.. don't worry, it'll get")
#          @window.puts("better next version! Stay tuned!")
#
#        end
#      end
#    )
  end

  def tunnel_data_incoming(packet)
    response = ViaSocket.handle(packet)
    if(!response.nil?)
      _send_request(response, nil)
    end
  end

  def tunnels_stop()
    if(@tunnels.length > 0)
      @window.puts("Stopping active tunnels...")
      @tunnels.each do |t|
        t.kill()
      end
    end
  end
end
