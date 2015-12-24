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
  def _register_commands_tunnels()
    @tunnels_by_session = {}
    @sessions_by_tunnel = {}
    @tunnels = []

    @commander.register_command('tunnels',
      Trollop::Parser.new do
        banner("Lists all current listeners")
      end,

      Proc.new do |opts, optarg|
        @tunnels.each do |tunnel|
          @window.puts(tunnel.to_s)
        end
      end
    )

    @commander.register_command('listen',
      Trollop::Parser.new do
        banner("Listens on a local port and sends the connection out the other side (like ssh -L). Usage: listen [<lhost>:]<lport> <rhost>:<rport>")
      end,

      Proc.new do |opts, optarg|
        local, remote = optarg.split(/ /)

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

        @window.puts("Listening on #{local_host}:#{local_port}, sending connections to #{remote_host}:#{remote_port}")

        begin
          @tunnels << Socketer.listen(local_host, local_port, {
            :on_connect => Proc.new() do |session, host, port|
              @window.puts("Connection from #{host}:#{port}; forwarding to #{remote_host}:#{remote_port}...")

              packet = CommandPacket.new({
                :is_request => true,
                :request_id => request_id(),
                :command_id => CommandPacket::TUNNEL_CONNECT,
                :options    => 0,
                :host       => remote_host,
                :port       => remote_port,
              })

              _send_request(packet, Proc.new() do |request, response|
                if(response.get(:command_id) == CommandPacket::COMMAND_ERROR)
                  @window.puts("Tunnel error: #{response.get(:reason)}")
                  session.stop!()
                else
                  @window.puts("[Tunnel %d] connection successful!" % response.get(:tunnel_id))
                  @tunnels_by_session[session] = response.get(:tunnel_id)
                  @sessions_by_tunnel[response.get(:tunnel_id)] = session

                  # Tell the tunnel that we're ready to receive data
                  session.ready!()
                end
              end)
            end,

            :on_data => Proc.new() do |session, data|
              tunnel_id = @tunnels_by_session[session]

              packet = CommandPacket.new({
                :is_request => true,
                :request_id => request_id(),
                :command_id => CommandPacket::TUNNEL_DATA,
                :tunnel_id => tunnel_id,
                :data => data,
              })

              _send_request(packet, nil)
            end,

            :on_error => Proc.new() do |session, msg, e|
              # Delete the tunnel
              tunnel_id = @tunnels_by_session.delete(session)
              @window.puts("[Tunnel %d] error: %s" % [tunnel_id, msg])

              @sessions_by_tunnel.delete(tunnel_id)

              packet = CommandPacket.new({
                :is_request => true,
                :request_id => request_id(),
                :command_id => CommandPacket::TUNNEL_CLOSE,
                :tunnel_id  => tunnel_id,
                :reason     => "Error during the connection: %s" % msg,
              })

              _send_request(packet, nil)
            end
          })
        rescue Errno::EACCES => e
          @window.puts("Sorry, couldn't listen on that port: #{e}")
        rescue Errno::EADDRINUSE => e
          @window.puts("Sorry, that address:port is already in use: #{e}")
          @window.puts()
          @window.puts("If you kill a session from the root window with the 'kill'")
          @window.puts("command, it will free the socket. You can get a list of which")
          @window.puts("sockets are being used with the 'tunnels' command!")
          @window.puts()
          @window.puts("I realize this is super awkward.. don't worry, it'll get")
          @window.puts("better next version! Stay tuned!")

        end
      end
    )
  end

  def tunnel_data_incoming(packet)
    tunnel_id = packet.get(:tunnel_id)

    case packet.get(:command_id)
    when CommandPacket::TUNNEL_DATA
      session = @sessions_by_tunnel[tunnel_id]
      if(session.nil?)
        @window.puts("Received data for unknown tunnel: %d! Telling client to close it!" % tunnel_id)

        _send_request(CommandPacket.new({
          :is_request => true,
          :request_id => request_id(),
          :command_id => CommandPacket::TUNNEL_CLOSE,
          :tunnel_id  => tunnel_id,
          :reason     => "Unknown tunnel: %d" % tunnel_id
        }), nil)
      else
        session.send(packet.get(:data))
      end
    when CommandPacket::TUNNEL_CLOSE
      @window.puts("[Tunnel %d] closed by the other side: %s!" % [tunnel_id, packet.get(:reason)])
      # Delete the tunnels, we're done with them
      session = @sessions_by_tunnel.delete(tunnel_id)
      if(session.nil?)
        @window.puts("WARNING: Client tried to close a tunnel that wasn't open (it may have just disconnected)")
        return
      end

      @tunnels_by_session.delete(session)
      session.stop!()
    else
      raise(DnscatException, "Unknown command sent by the server: #{packet}")
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
