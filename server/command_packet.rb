##
# command_packet_stream.rb
# Created May, 2014
# By Ron Bowes
#
# See: LICENSE.txt
#
##

require 'dnscat_exception'

class CommandPacket
  COMMAND_PING  = 0x0000
  COMMAND_SHELL = 0x0001
  COMMAND_EXEC  = 0x0002

  attr_reader :request_id, :command_id, :status # header
  attr_reader :data # ping
  attr_reader :name, :session_id # shell
  attr_reader :command # command

  def at_least?(data, needed)
    return (data.length >= needed)
  end

  def parse_header(data)
    at_least?(data, 6) || raise(DnscatException, "Command packet is too short (header)")

    # (uint8_t) message_type
    # (uint16_t) session_id
    @type, @session_id = data.unpack("Cn")

    return data[3..-1]
  end

  def initialize(data)
    @data = data
  end

  def to_s()
    return @data
  end

end
