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
  attr_reader :lhost, :lport

  BUFFER = 65536

  class Manager
    attr_reader :name

    def initialize(s, callbacks = {})
      @s          = s
      @on_connect = callbacks[:on_connect]
      @on_ready   = callbacks[:on_ready]
      @on_data    = callbacks[:on_data]
      @on_error   = callbacks[:on_error]

#      if(@on_connect)
#        @on_connect.call(self)
#      end
    end

    def _handle_exception(e, msg)
      if(@on_error)
        @on_error.call(self, "Error #{msg}: #{e}", e)
      end
      close()
    end

    def ready!()
      @thread = Thread.new() do
        begin
          loop do
            data = @s.recv(BUFFER)

            if(data.nil? || data.length == 0)
              raise(IOError, "Connection closed")
            end

            if(@on_data)
              @on_data.call(self, data)
            end
          end
        rescue StandardError => e
          puts(e.backtrace)
          begin
            _handle_exception(e, "receiving data")
          rescue StandardError => e
            puts("Error in exception handler; please don't do that :)")
            puts(e)
            puts(e.backtrace)
          end
        end
      end

      # Do the 'new connection' callback
      if(@on_ready)
        @on_ready.call(self, name)
      end
    end

    def close()
      if(@thread)
        @thread.exit()
      end

      @s.close()
    end

    def write(data)
      begin
        @s.write(data)
      rescue Exception => e
        _handle_exception(e, "sending data")

        close()
      end
    end
  end

  class Listener
    def initialize(lhost, lport, callback)
      # Start the socket right away so we can catch errors quickly
      @s     = TCPServer.new(lhost, lport)
      @lhost = lhost
      @lport = lport
      @thread = Thread.new() do
        begin
          loop do
            callback.call(@s.accept())
          end
        rescue StandardError => e
          puts("Error in Listener: #{e}")
        end
      end
    end

    def to_s()
      return "Socketer::Listener on %s:%d" % [@lhost, @lport]
    end

    def kill()
      begin
        if(@thread)
          @thread.exit()
        end
        @s.close()
      rescue
        # Ignore exceptions
      end
    end
  end
end
