##
# command_packet.rb
# Created May, 2014
# By Ron Bowes
#
# See: LICENSE.md
##

require 'libs/dnscat_exception'
require 'libs/hex'

##
# Defines a packet in the "command protocol". There's a document in the docs/
# folder that describes the command protocol, but essentially it's a protocol
# that runs on top of the dnscat2 protocol and provides functionality like
# uploading/downloading files, spawning a shell, etc.
#
# By default, when a new session is created, it's a "command session", where
# on the server, a Meterpreter-like menu is given.
##
class CommandPacket
  COMMAND_PING     = 0x0000
  COMMAND_SHELL    = 0x0001
  COMMAND_EXEC     = 0x0002
  COMMAND_DOWNLOAD = 0x0003
  COMMAND_UPLOAD   = 0x0004
  COMMAND_SHUTDOWN = 0x0005
  COMMAND_ERROR    = 0xFFFF

  COMMAND_NAMES = {
    0x0000 => "COMMAND_PING",
    0x0001 => "COMMAND_SHELL",
    0x0002 => "COMMAND_EXEC",
    0x0003 => "COMMAND_DOWNLOAD",
    0x0004 => "COMMAND_UPLOAD",
    0x0005 => "COMMAND_SHUTDOWN",
    0xFFFF => "COMMAND_ERROR",
  }

  # These are used in initialize() to make sure the caller is passing in the
  # right fields
  VALIDATORS = {
    COMMAND_PING => {
      :request  => [ :data ],
      :response => [ :data ],
    },
    COMMAND_SHELL => {
      :request  => [ :name ],
      :response => [ :session_id ],
    },
    COMMAND_EXEC => {
      :request  => [ :name, :command ],
      :response => [ :session_id ],
    },
    COMMAND_DOWNLOAD => {
      :request  => [ :filename ],
      :response => [ :data ],
    },
    COMMAND_UPLOAD => {
      :request  => [ :filename, :data ],
      :response => [],
    },
    COMMAND_SHUTDOWN => {
      :request  => [],
      :response => [],
    },
    COMMAND_ERROR => {
      :request  => [ :status, :reason ],
      :response => [ :status, :reason ],
    }
  }

  def CommandPacket._at_least?(data, needed)
    if(data.length < needed)
      raise(DnscatException, "Command packet validation failed: data wasn't long enough (needed at least #{needed}, only had #{data.length}).")
    end
  end

  def CommandPacket._null_terminated?(data)
    if(data.index("\0").nil?)
      raise(DnscatException, "Command packet validation failed: data wasn't NUL terminated.")
    end
  end

  def CommandPacket._done?(data)
    if(data.length > 0)
      raise(DnscatException, "Command packet validation failed: there was extra data on the end of the packet")
    end
  end

  def set(name, value)
    @data[name] = value
  end

  def get(name)
    return @data[name]
  end

  def CommandPacket.ready?(packet)
    if(packet.length < 4)
      return false
    end

    length, packet = packet.unpack("Na*")
    return (packet.length >= length)
  end

  def CommandPacket.parse(packet, is_request)
    _at_least?(packet, 4)
    data = {
      :is_request => is_request
    }

    data[:request_id], data[:command_id], packet = packet.unpack("nna*")

    case data[:command_id]
    when COMMAND_PING
      _null_terminated?(packet)
      data[:data], packet = packet.unpack("Z*a*")

    when COMMAND_SHELL
      if(data[:is_request])
        _null_terminated?(packet)
        data[:name], packet = packet.unpack("Z*a*")
      else
        _at_least?(packet, 2)
        data[:session_id], packet = packet.unpack("na*")
      end

    when COMMAND_EXEC
      if(data[:is_request])
        _null_terminated?(packet)
        data[:command], packet = packet.unpack("Z*a*")
      else
        _at_least?(packet, 2)
        data[:session_id], packet = packet.unpack("na*")
      end

    when COMMAND_DOWNLOAD
      if(data[:is_request])
        _null_terminated?(packet)
        data[:filename], packet = packet.unpack("Z*a*")
      else
        data[:data], packet = packet.unpack("a*a0")
      end

    when COMMAND_UPLOAD
      if(data[:is_request])
        _null_terminated?(packet)
        data[:filename], packet = packet.unpack("Z*a*")
        data[:data], packet = packet.unpack("a*a0")
      else
        # n/a
      end
    when COMMAND_SHUTDOWN
      # n/a - there's no data in either direction

    when COMMAND_ERROR
      _at_least?(packet, 2)
      data[:status], packet = packet.unpack("na*")

      _null_terminated?(packet)
      data[:reason], packet = packet.unpack("Z*a*")
    end

    _done?(packet)

    return CommandPacket.new(data)
  end

  # Verifies that all required fields for the type are present.
  # :command_id, :request_id, and :is_request are all required.
  def validate(data)
    # Make sure they passed in a command_id and request_id
    if(data[:command_id].nil?)
      raise(DnscatException, "Required field missing: :command_id")
    end
    if(data[:request_id].nil?)
      raise(DnscatException, "Required field missing: :request_id")
    end

    # Find a validator for this
    validator = VALIDATORS[data[:command_id]]
    if(validator.nil?)
      raise(DnscatException, "Unknown command_id (#{data[:command_id]}) or missing validator")
    end

    # Make sure they set is_request and get the appropriate validator
    if(data[:is_request].nil?)
      raise(DnscatException, "Required field missing: :is_request")
    end
    validator = data[:is_request] ? validator[:request] : validator[:response]

    # Make sure all the fields in the validator are present
    validator.each do |f|
      if(data[f].nil?)
        raise(DnscatException, "Required field missing: #{f}")
      end
    end
  end

  # Data is simply a hash of the required fields (see the VALIDATORS variable
  # for which fields are required for each message type).
  def initialize(data)
    validate(data)
    @data = data.clone()
  end

  # Convert to a byte string
  def serialize()
    validate(@data)

    packet = ""
    packet += [@data[:request_id], @data[:command_id]].pack("nn")

    case @data[:command_id]
    when COMMAND_PING
      packet += [@data[:data]].pack("Z*")

    when COMMAND_SHELL
      if(@data[:is_request])
        packet += [@data[:name]].pack("Z*")
      else
        packet += [@data[:session_id]].pack("n")
      end

    when COMMAND_EXEC
      if(@data[:is_request])
        packet += [@data[:name]].pack("Z*")
        packet += [@data[:command]].pack("Z*")
      else
        packet += [@data[:session_id]].pack("n")
      end

    when COMMAND_DOWNLOAD
      if(@data[:is_request])
        packet += [@data[:filename]].pack("Z*")
      else
        packet += [@data[:data]].pack("a*")
      end

    when COMMAND_UPLOAD
      if(@data[:is_request])
        packet += [@data[:filename], @data[:data]].pack("Z*a*")
      else
        # n/a
      end
    when COMMAND_SHUTDOWN
      # n/a - there's no data in either direction

    when COMMAND_ERROR
      packet += [@data[:status], @data[:reason]].pack("nZ*")
    end

    return packet
  end

  # Convert to a user-readable string
  def to_s()
    response = "%s :: %s\n" % [COMMAND_NAMES[@data[:command_id]], @data.to_s()]
    response += Hex.to_s(self.serialize())

    return response
  end
end
