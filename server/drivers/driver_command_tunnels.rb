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
  @@sockets = {}

  def initialize(driver, host, port, callbacks)
    @tunnel_id = nil
    @session   = nil

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
      v_socket, s_socket = UNIXSocket.pair()

      # Get the tunnel_id
      @tunnel_id = response.get(:tunnel_id)

      # If the response was good, then we can create a Socketer session and hook up to it!
      @session = Socketer::Session.new(s_socket, "#{host}:#{port} via tunnel #{@tunnel_id}", callbacks)

      # Save ourselves in the list of instances
      @@sockets[@tunnel_id] = v_socket

      # Start a receive thread for the socket
      @thread = Thread.new() do
        loop do
          data = v_socket.recv(Socketer::BUFFER)

          driver._send_request(CommandPacket.new({
            :is_request => true,
            :request_id => driver.request_id(),
            :command_id => CommandPacket::TUNNEL_DATA,
            :tunnel_id  => @tunnel_id,
            :data       => data,
          }), nil)
        end
      end


#      @tunnels[tunnel_id] = {
#        :thread  => thread,
#        :socket  => v_socket,
#      }
#
#      # Do a special callback
#      if(callbacks[:on_via])
#        callbacks[:on_via].call(tunnel_id)
#      end

      # We're only ready AFTER we are prepared to receive data - this may do a callback instantly
      @session.ready!()
    end)
  end

  def wait_for_session()
    while(@session.nil?)
      # TODO: Get rid of this
      puts("Waiting for the tunnel to connect...")
      sleep(0.1)
    end
  end

  def write(data)
    wait_for_session()

    @session.send(data)
  end

  def close()
    @session.stop!()
    @thread.exit()
  end

  def ViaSocket.handle_packet(packet)
    tunnel_id = packet.get(:tunnel_id)
    socket = @@sockets[tunnel_id]
    if(socket.nil?)
      puts("ERROR: Couldn't find the socket for tunnel #{tunnel_id}")

      _send_request(CommandPacket.new({
        :is_request => true,
        :request_id => request_id(),
        :command_id => CommandPacket::TUNNEL_CLOSE,
        :tunnel_id  => tunnel_id,
        :reason     => "Unknown tunnel: %d" % tunnel_id
      }), nil)
    end

    case packet.get(:command_id)
    when CommandPacket::TUNNEL_DATA
      socket.write(packet.get(:data))

    when CommandPacket::TUNNEL_CLOSE
      socket.close()
    else
      raise(DnscatException, "Unknown command sent by the server: #{packet}")
    end
  end
end

module DriverCommandTunnels
  def _register_commands_tunnels()
    @tunnels = []
    @sessions = {}

    @commander.register_command('wget',
      Trollop::Parser.new do
        banner("Perform an HTTP download via an established tunnel")
      end,

      Proc.new do |opts, optarg|
        ViaSocket.new(self, 'localhost', 4444, {
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
#            # This indent-level is the callbacks for the LOCAL socket (the
#            # socket that the server is listening on)
#            :on_connect => Proc.new() do |session, name|
#              @window.puts("Connection from #{name}; forwarding to #{remote_host}:#{remote_port}...")
#
#              # These are the callbacks for the REMOTE socket (the fake socket that's on the remote side)
#              create_via_socket('localhost', 4444, {
#                :on_via => Proc.new() do |tunnel_id|
#                  @sessions[session] = tunnel_id
#                end,
#                :on_ready => Proc.new() do |v_session, v_name|
#                  @window.puts("[Tunnel #{@sessions[session]}] Tunnel ready: #{v_name}")
#                end,
#                :on_data => Proc.new() do |v_session, v_data|
#                  @window.puts("[Tunnel #{@sessions[session]}] Data arrived from the other side! #{v_data.length} bytes")
#                  session.write(v_data)
#                end,
#                :on_error => Proc.new() do |v_session, v_msg, v_e|
#                  @window.puts("[Tunnel #{@sessions[session]}] Tunnel closed: #{v_msg} #{v_e}")
#                end,
#              })
#
#              session.ready!()
#            end,
#
#            :on_data => Proc.new() do |session, data|
#              tunnel_id = @sessions[session]
#              if(tunnel_id.nil?)
#                @window.puts("[Tunnel #{tunnel_id}] Unknown session!")
#                next
#              end
#
#              via_socket = @tunnels[tunnel_id]
#              if(via_socket.nil?)
#                @window.puts("[Tunnel #{tunnel_id}] Unknown tunnel!")
#                next
#              end
#
#              via_socket[:socket].write(data)
#            end,
#
#            :on_error => Proc.new() do |session, msg, e|
#              tunnel_id = @sessions[session]
#              if(tunnel_id.nil?)
#                @window.puts("[Tunnel #{tunnel_id}] Error in unknown session!")
#                next
#              end
#
#              via_socket = @tunnels[tunnel_id]
#              if(via_socket.nil?)
#                @window.puts("[Tunnel #{tunnel_id}] Error in unknown tunnel!")
#                next
#              end
#
#              # TODO: Kill the session properly
#            end
#          })
#        rescue Errno::EACCES => e
#          @window.puts("Sorry, couldn't listen on that port: #{e}")
#        rescue Errno::EADDRINUSE => e
#          @window.puts("Sorry, that address:port is already in use: #{e}")
#          # TODO: Better error msg
#        end
#      end
#    )
  end

  def tunnel_data_incoming(packet)
    ViaSocket.handle_packet(packet)
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
