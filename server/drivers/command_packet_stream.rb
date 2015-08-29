##
# command_packet_stream.rb
# Created May, 2014
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'libs/dnscat_exception'

class CommandPacketStream
  def initialize()
    @data = ""
    @length = nil
  end

  def feed(data, is_request)
    # Add the new data to our current stream
    @data = @data || ""
    @data += data

    # Process everything we can
    loop do
      # If we don't have enough data, return immediately
      if(@data.length < 4)
        return nil
      end

      # Try to read a length + packet
      length, data = @data.unpack("Na*")

      # If there isn't enough data, give up
      if(data.length < length)
        return nil
      end

      # Otherwise, remove what we have from @data
      length, data, @data = @data.unpack("Na#{length}a*")

      # And that's it!
      return CommandPacket.new(data, is_request)
    end
  end
end
