##
# socketer.rb
# Created November 27, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
# This is a fairly simple wrapper around TCPSocket that makes the interface
# a bit more ruby-esque.
##

require 'socket'

class Socketer
  BUFFER = 65536

  class Session
    def initialize(client, callbacks = {})
      @client     = client
      @on_connect = callbacks[:on_connect]
      @on_data    = callbacks[:on_data]
      @on_error   = callbacks[:on_error]

      @thread = Thread.new() do |t|
        #puts("Starting recv thread...")

        begin
          # Do the 'new connection' callback
          if(@on_connect)
            @on_connect.call(self, client.peeraddr[2], client.peeraddr[1])
          end

          loop do
            data = @client.recv(BUFFER)

            if(data.nil?)
              if(@on_error)
                @on_error.call(self, "Connection closed")
              end

              # Exit the loop
              break
            end
            if(@on_data)
              @on_data.call(self, data)
            end
          end
        rescue Exception => e
          #puts(e)
          #puts(e.backtrace)
          if(@on_error)
            @on_error.call(self, "Connection error: #{e}")
          end
        end

        @client.close()
        #puts("Done")
      end
    end

    def send(data)
      begin
        @client.write(data)
      rescue Exception => e
        if(@on_error)
          @on_error.call(self, "Connection error: #{e}")
        end
        @thread.exit()
      end
    end
  end

  def Socketer.listen(host, port, callbacks = {})
    # Create the socket right away so we'll know if it fails
    #puts("Listening on #{host}:#{port}...")
    s = TCPServer.new(host, port)

    return Thread.new() do
      loop do
        # TODO: begin
        Session.new(s.accept(), callbacks)
      end
    end
  end

  def Socketer.connect(host, port, callbacks = {})
    #puts("Connecting to #{host}:#{port}...")

    return Thread.new() do
      begin
        s = TCPSocket.new(host, port)
        if(s)
          Session.new(s, callbacks)
        else
          if(callbacks[:on_error])
            callbacks[:on_error].call(nil, "Couldn't connect to #{host}:#{port}!")
          end
        end
      rescue Exception => e
        if(callbacks[:on_error])
          callbacks[:on_error].call(nil, "Error connecting to #{host}:#{port}: #{e}")
        end
      end
    end
  end
end

# TODO: Get rid of these examples
#puts("Listening...")
#listener = Socketer.listen('0.0.0.0', 1234, {
#  :on_connect => Proc.new() do |session, host, port|
#    puts("Connection: #{session}")
#    session.send("Welcome!\n")
#  end,
#  :on_data => Proc.new() do |session, data|
#    puts("Data: #{session}")
#    session.send("#{data.length} :: #{data}")
#  end,
#  :on_error => Proc.new() do |session, msg|
#    puts("Error: #{msg}")
#  end
#})
#listener.join()

#puts("Connecting...")
#connector = Socketer.connect("www.javaop.com", 80, {
#  :on_connect => Proc.new() do |session, host, port|
#    puts("Connected to #{host}:#{port}!")
#    session.send("HEAD / HTTP/1.0\r\nHost: www.javaop.com\r\n\r\n")
#  end,
#  :on_data => Proc.new() do |session, data|
#    data.split(/\n/).each do |line|
#      puts(line)
#    end
#  end,
#  :on_error => Proc.new() do |session, msg|
#    puts("Error: #{msg}")
#  end
#})

sleep(1000)
