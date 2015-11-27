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


        buffers = {}
        session_tunnel_map = {}
        Socketer.listen(local_host, local_port, {
          :on_connect => Proc.new() do |session, host, port|
            @window.puts("Got a connection from #{host}:#{port}, asking the other side to connect for us...")

            connect = CommandPacket.new({
              :is_request => true,
              :request_id => request_id(),
              :command_id => CommandPacket::TUNNEL_CONNECT,
              :host       => remote_host,
              :port       => remote_port,
            })

            _send_request(connect) do |request, response|
              # TODO: Check response
              sessions[session] = response.get(:tunnel_id)
              if(buffers[session])
                @window.puts("TODO: Send buffered data")
              end
            end
          end,
          :on_data => Proc.new() do |session, data|
            @window.puts("Received data!")
            if(session_tunnel_map[session])
              # TODO: Send it across the tunenl
            else
              @window.puts("Server hasn't responded yet, buffering...")
              buffers[session] = (buffers[session] || "") + data
            end
          end,
          :on_error => Proc.new() do |session, msg|
            @window.puts("Error in listener tunnel: #{msg}")
          end
        })
      end
    )
  end
end
