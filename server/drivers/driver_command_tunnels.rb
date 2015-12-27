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
      @v_socket, s_socket = UNIXSocket.pair()

      # Get the tunnel_id
      @tunnel_id = response.get(:tunnel_id)

      # If the response was good, then we can create a Socketer session and hook up to it!
      @session = Socketer::Session.new(s_socket, "#{host}:#{port} via tunnel #{@tunnel_id}", callbacks)

      # Save ourselves in the list of instances
      @@via_sockets[@tunnel_id] = self

      # Start a receive thread for the socket
      @thread = Thread.new() do
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
      end

      if(callbacks[:on_via])
        callbacks[:on_via].call(@session, self)
      end

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

  def _sneak_in_data(data)
    @v_socket.write(data)
  end

  def ViaSocket.get(driver, tunnel_id)
    via_socket = @@via_sockets[tunnel_id]
    if(via_socket.nil?)
      puts("ERROR: Couldn't find the socket for tunnel #{tunnel_id}")

      driver._send_request(CommandPacket.new({
        :is_request => true,
        :request_id => request_id(),
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

    @commander.register_command('listen',
      Trollop::Parser.new do
        banner("Listens on a local port and sends the connection out the other side (like ssh -L). Usage: listen [<lhost>:]<lport> <rhost>:<rport>")
      end,

      Proc.new do |opts, optarg|
        lhost, lport, rhost, rport = _parse_host_ports(optarg)
        @window.puts("Listening on #{lhost}:#{lport}, sending connections to #{rhost}:#{rport}")

        begin
          # This listens on the server side, and creates a new ViaSocket for each connection
          Socketer.listen(lhost, lport, {
            # This indent-level is the callbacks for the SERVER socket (the
            # socket that the local server is listening on)
            :on_connect => Proc.new() do |session, name|
              @window.puts("Connection from #{name}; forwarding to #{rhost}:#{rport}...")

              # These are the callbacks for the REMOTE socket (the fake socket that's on the client side)
              ViaSocket.new(self, rhost, rport, {
                :on_via => Proc.new() do |v_session, via_socket|
                  # Index the via_socket in such a way that the SERVER socketer instance can find it
                  puts("Saving socket #{via_socket} under session #{session}")
                  @via_sockets[session] = via_socket
                end,
                :on_ready => Proc.new() do |v_session, v_name|
                  via_socket = @via_sockets[session]
                  @window.puts("[Tunnel #{via_socket.tunnel_id}] Tunnel ready: #{v_name}")
                  # Tell the SERVER session that we're ready to start receiving data
                  session.ready!()
                end,
                :on_data => Proc.new() do |v_session, v_data|
                  via_socket = @via_sockets[session]
                  @window.puts("[Tunnel #{via_socket.tunnel_id}] Data arrived from the other side! #{v_data.length} bytes")
                  # Write data from the REMOTE socket to the SERVER socket
                  session.send(v_data)
                end,
                :on_error => Proc.new() do |v_session, v_msg, v_e|
                  via_socket = @via_sockets[session]
                  @window.puts("[Tunnel #{via_socket.tunnel_id}] Tunnel closed: #{v_msg} #{v_e}")
                  v_session.close!()
                  session.close!()
                end,
              })
            end,

            :on_data => Proc.new() do |session, data|
              via_socket = @via_sockets[session]
              if(via_socket.nil?)
                @window.puts("[Tunnel #{tunnel_id}] Unknown session!")
                next
              end

              via_socket.write(data)
            end,

            :on_error => Proc.new() do |session, msg, e|
              puts("Looking up socket for session #{session}...")
              via_socket = @via_sockets[session]
              if(via_socket.nil?)
                @window.puts("[Tunnel ???] Unknown session had an error: #{msg} #{e}")
                next
              end

              via_socket.close()
            end
          })
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
