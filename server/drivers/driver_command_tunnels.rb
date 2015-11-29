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

    @commander.register_command('listen',
      Trollop::Parser.new do
        banner("Listens on a local port and sends the connection out the other side (like ssh -L). Usage: listen [<host>:]<port> <host>:<port>")
      end,

      Proc.new do |opts, optarg|
        local, remote = optarg.split(/ /)

        if(local.include?(":"))
          local_host, local_port = local.split(/:/)
        else
          local_host = '0.0.0.0'
          local_port = local
        end
        local_port = local_port.to_i()

        remote_host, remote_port = remote.split(/:/)
        if(remote_host == '' || remote_port == '')
          # TODO: Raise
          return
        end

        Socketer.listen(local_host, local_port, {
          :on_connect => Proc.new() do |session, host, port|
            @window.puts("Got a connection from #{host}:#{port}, asking the other side to connect for us...")

            packet = CommandPacket.new({
              :is_request => true,
              :request_id => request_id(),
              :command_id => CommandPacket::TUNNEL_CONNECT,
              :host       => remote_host,
              :port       => remote_port,
            })

            _send_request(packet) do |request, response|
              @window.puts("Other side connected, stream #{response.get(:tunnel_id)}")
              @tunnels_by_session[session] = response.get(:tunnel_id)
              @sessions_by_tunnel[response.get(:tunnel_id)] = session

              # Tell the tunnel that we're ready to receive data
              session.ready!()
            end
          end,

          :on_data => Proc.new() do |session, data|
            tunnel_id = @tunnels_by_session[session]

            @window.puts("Received data on the local socket! Sending to tunnel #{tunnel_id}...")

            packet = CommandPacket.new({
              :is_request => true,
              :request_id => request_id(),
              :command_id => CommandPacket::TUNNEL_DATA,
              :tunnel_id => tunnel_id,
              :data => data,
            })

            _send_request(packet)
          end,
          :on_error => Proc.new() do |session, msg|
            @window.puts("Error in listener tunnel: #{msg}")

            # Delete the tunnels, we're done with them
            tunnel_id = @tunnels_by_session.delete(session)
            @sessions_by_tunnel.delete(tunnel_id)

            packet = CommandPacket.new({
              :is_request => true,
              :request_id => request_id(),
              :command_id => CommandPacket::TUNNEL_CLOSE,
              :tunnel_id => tunnel_id,
            })

            _send_request(packet)
          end
        })
      end
    )
  end

  def tunnel_data_incoming(packet)
    tunnel_id = packet.get(:tunnel_id)

    case packet.get(:command_id)
    when CommandPacket::TUNNEL_DATA
      session = @sessions_by_tunnel[tunnel_id]
      if(session.nil?)
        @window.puts("Got a packet for an unknown session! Sending a close signal.")

        _send_request(CommandPacket.new({
          :is_request => true,
          :request_id => request_id(),
          :command_id => CommandPacket::TUNNEL_CLOSE,
          :tunnel_id => tunnel_id,
        }))
      else
        session.send(packet.get(:data))
      end
    when CommandPacket::TUNNEL_CLOSE
      # Delete the tunnels, we're done with them
      session = @sessions_by_tunnel.delete(tunnel_id)
      @tunnels_by_session.delete(session)
    else
      @window.puts("Unknown command sent to tunnel_data_incoming: #{packet.get(:command_id)}")
    end
  end
end
