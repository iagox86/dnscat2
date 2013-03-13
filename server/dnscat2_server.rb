$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'socket'

require 'session'
require 'packet'

# This class should be totally stateless, and rely on the Session class
# for any long-term session storage
class Dnscat2
  def Dnscat2.handle_syn(packet, session)
    puts("Handling SYN!")
    exit
  end

  def Dnscat2.go(s)
    session_id = nil
    begin
      loop do
        packet = Packet.read(s)
        session = Session.find(packet.session_id)

        puts(packet.inspect)

        if(packet.type == Packet::MESSAGE_TYPE_SYN)
          handle_syn(packet, session)
        elsif(packet.type == Packet::MESSAGE_TYPE_MSG)
          handle_msg(packet, session)
        elsif(packet.type == Packet::MESSAGE_TYPE_FIN)
          handle_fin(packet, session)
        else
          raise(IOError, "Unknown packet type: #{packet.type}")
        end

        puts("Done!")
        exit
      end
    rescue IOError => e
      puts("Exception: #{e}")

      if(!session_id.nil?)
        # TODO Send a FIN if we can
        Session.destroy(session_id)
      end

      puts(e.inspect)
      puts(e.backtrace)
    end

    s.close()
  end
end

class Test
  def initialize()
    @data = ''
  end

  def queue(data)
    @data += [data.length].pack("n")
    @data += data
  end

  def read(n)
    if(@data.length < n)
      raise(Exception, "No data left")
    end

    response = @data[0,n]
    @data = @data[n..-1]

    return response
  end
end

test = Test.new

test.queue( [
              Packet::MESSAGE_TYPE_SYN,  # Type
              0x1234,            # Session id
              0x0000,            # Options
              0x0000,            # Initial seq
            ].pack("Cnnn"))
Dnscat2.go(test)

#def get_socket_string(addr)
#  return "#{addr[3]}:#{addr[1]} (#{addr[2]})"
#end
#
#server = TCPServer.new(2000)
#
#puts("Server ready: #{get_socket_string(server.addr)}")
#loop do
#  Thread.start(server.accept) do |s|
#    begin
#      puts("Received a connection from #{get_socket_string(s.peeraddr)}")
#      go_tcp(s)
#    rescue Exception => e
#      puts(e.inspect)
#      puts(e.backtrace)
#    end
#  end
#end
