require 'socket'

class Tunnel
  def initialize(session_id, host, port)
    @session_id = session_id
    @s = TCPSocket.new(host, port)
    @thread = nil
  end

  def go()
    @thread = Thread.new() do
      begin
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
      rescue DnscatException => e
        Log.ERROR("Protocol exception caught in DNS module:")
        Log.ERROR(e.inspect)
      rescue Exception => e
        Log.FATAL("Fatal exception caught in DNS module:")
        Log.FATAL(e.inspect)
        Log.FATAL(e.backtrace)
        exit
      end
    end
  end

  def send(data)
    @s.write(data)
  end

  def kill()
    @thread.kill
    @s.close
  end
end
