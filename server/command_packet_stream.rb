##
# command_packet_stream.rb
# Created May, 2014
# By Ron Bowes
#
# See: LICENSE.txt
#
##

require 'dnscat_exception'
require 'command_packet'

class CommandPacketStream
  def initialize()
    @data = ""
    @length = nil
  end

  def feed(data)
    # Add the new data to our current stream
    @data = @data || ""
    @data += data

    # Process everything we can
    loop do
      # If we don't have enough data, return immediately
      if(@data.length < 2)
        return
      end

      # Try to read a length + packet
      length, data = @data.unpack("na*")

      # If there isn't enough data, give up
      if(data.length < length)
        return
      end

      # Otherwise, remove what we have from @data
      length, data, @data = @data.unpack("na#{length}a*")

      # And that's it!
      yield(CommandPacket.new(data))
    end
  end
end

# Test code
#s = CommandPacketStream.new()
#
## Try a perfect string
#str = "\x00\x0aAAAAAAAAAB"
#puts("Expected: CCCCCCCCCB")
#s.feed(str) do |command|
#  puts(command.to_s.unpack("H*"))
#end
#
## Try a string that spans multiple messages
#str = "\x00\x0aAAAAAAAAAB\x00\x0aBBBBBBBBBA"
#puts("Expected: two strings (AAAAAAAAAB and BBBBBBBBBA)")
#s.feed(str) do |command|
#  puts(command.to_s.unpack("H*"))
#end
#
## Try a string that splits the length, which could be tricky
#str = "\x00"
#puts("Expected: n/a")
#s.feed(str) do |command|
#  puts(command.to_s.unpack("H*"))
#end
#str = "\x0aDDDDDDDDDA"
#puts("Expected: DDDDDDDDDA")
#s.feed(str) do |command|
#  puts(command.to_s.unpack("H*"))
#end
#
## Finally, try 1.5 strings
#str = "\x00\x0aEEEEEEEEEF\x00\x0aGGGGG"
#puts("Expected: EEEEEEEEEF")
#s.feed(str) do |command|
#  puts(command.to_s.unpack("H*"))
#end
#str = "HHHHH"
#puts("Expected: GGGGGHHHHH")
#s.feed(str) do |command|
#  puts(command.to_s.unpack("H*"))
#end
#
