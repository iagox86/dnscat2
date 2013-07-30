require 'socket'

class Tunnel
  def initialize(session_id, host, port)
    @session_id = session_id
    @buffer = ""

    @thread = Thread.new() do
      begin
        @s = TCPSocket.new(host, port)
        if(@buffer.length > 0)
          @s.write(@buffer)
          @buffer = nil
        end

        loop do
          data = @s.recv(0xFFFF)
          if(data.nil?)
            Session.find(@session_id).destroy()
            break
          else
            Session.find(@session_id).queue_outgoing(data)
          end
        end
      rescue SystemExit
        exit
      rescue Exception => e
        Log.FATAL("Exception caught in Tunnel module:")
        Log.FATAL(e.inspect)
      end
    end
  end

  def send(data)
    if(@s.nil?)
      @buffer = @buffer + data
    else
      @s.write(data)
    end
  end

  def kill()
    @thread.kill
    if(!@s.nil?)
      @s.close
    end
  end
end
