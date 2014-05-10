##
# command_packet_stream.rb
# Created May, 2014
# By Ron Bowes
#
# See: LICENSE.txt
#
##

# TODO: Remove this
$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'dnscat_exception'

require 'command_packet_stream'

class CommandPacket
  COMMAND_PING  = 0x0000
  COMMAND_SHELL = 0x0001
  COMMAND_EXEC  = 0x0002

  attr_reader :request_id, :command_id, :status # header
  attr_reader :data # ping
  attr_reader :name, :session_id # shell
  attr_reader :command # command

  attr_reader :reason # errors

  def at_least?(data, needed)
    return (data.length >= needed)
  end

  def is_error?()
    return is_response?() && @status != 0
  end

  def is_request?()
    return @is_request
  end

  def is_response?()
    return !@is_request
  end

  def parse_ping(data, is_request)
    if(data.index("\0").nil?)
      raise(DnscatException, "Ping packet doesn't end in a NUL byte")
    end

    @data, data = data.unpack("Z*a*")
    if(data.length > 0)
      raise(DnscatException, "Ping packet has extra data on the end")
    end
  end

  def parse_shell(data, is_request)
    if(is_request)
      if(data.index("\0").nil?)
        raise(DnscatException, "Shell packet request doesn't have a NUL byte")
      end
      @name, data = data.unpack("Z*a*")
    else
      if(data.length < 2)
        raise(DnscatException, "Shell packet response doesn't have a SessionID")
      end
      @session_id, data = data.unpack("na*")
    end

    if(data.length > 0)
      raise(DnscatException, "Shell packet has extra data on the end")
    end
  end

  def parse_exec(data, is_request)
    if(is_request)
      if(data.index("\0").nil?)
        raise(DnscatException, "Exec packet request doesn't have a NUL byte after name")
      end
      @command, data = data.unpack("Z*a*")
      if(data.index("\0").nil?)
        raise(DnscatException, "Exec packet request doesn't have a NUL byte after command")
      end
      @command, data = data.unpack("Z*a*")
    else
      if(data.length < 2)
        raise(DnscatException, "Exec packet response doesn't have a SessionID")
      end
      @session_id, data = data.unpack("na*")
    end

    if(data.length > 0)
      raise(DnscatException, "Exec packet has extra data on the end")
    end
  end

  def parse_error(data)
    if(data.index("\0").nil?)
      raise(DnscatException, "Exec packet request doesn't have a NUL byte after name")
    end
    @reason, data = data.unpack("Z*a*")

    if(data.length > 0)
      raise(DnscatException, "Exec packet has extra data on the end")
    end
  end

  def initialize(data, is_request)
    # The length is already handled by command_packet_stream
    # This is the length of the header
    at_least?(data, 4) || raise(DnscatException, "Command packet is too short (header)")

    # Store whether or not it's a request
    @is_request = is_request

    # (uint16_t) request_id
    @request_id, data = data.unpack("na*")

    # (uint16_t) command_id
    @command_id, data = data.unpack("na*")

    if(!is_request)
      # (uint16_t) status (response only)
      @status, data = data.unpack("na*")

      # If the status was non-zero, parse the message as an error
      if(@status != 0)
        return false, parse_error(data)
      end
    end

    if(@command_id == COMMAND_PING)
      parse_ping(data, is_request)
    elsif(@command_id == COMMAND_SHELL)
      parse_shell(data, is_request)
    elsif(@command_id == COMMAND_EXEC)
      parse_exec(data, is_request)
    else
      raise(DnscatException, "Unknown command: 0x%04x" % @command_id)
    end
  end

  def CommandPacket.add_header(packet, request_id, command_id, status = nil)
    if(status.nil?)
      packet = [request_id, command_id, packet].pack("nna*")
    else
      packet = [request_id, command_id, status, packet].pack("nnna*")
    end

    return [packet.length, packet].pack('na*')
  end

  def CommandPacket.create_ping_request(request_id, data)
    return CommandPacket.add_header([data].pack('Z*'), request_id, COMMAND_PING)
  end
  def CommandPacket.create_ping_response(request_id, data)
    return CommandPacket.add_header([data].pack('Z*'), request_id, COMMAND_PING, 0)
  end

  def CommandPacket.create_shell_request(request_id, name)
    return CommandPacket.add_header([name].pack('Z*'), request_id, COMMAND_SHELL)
  end
  def CommandPacket.create_shell_response(request_id, session_id)
    return CommandPacket.add_header([session_id].pack('n'), request_id, COMMAND_SHELL, 0)
  end

  def CommandPacket.create_exec_request(request_id, name, command)
    return CommandPacket.add_header([name, command].pack('Z*Z*'), request_id, COMMAND_EXEC)
  end
  def CommandPacket.create_exec_response(request_id, session_id)
    return CommandPacket.add_header([session_id].pack('n'), request_id, COMMAND_EXEC, 0)
  end

  def CommandPacket.create_error(request_id, reason, status)
    if(status == 0)
      raise(DnscatException, "Tried to create an error packet with a status of 0")
    end

    return CommandPacket.add_header([reason].pack("Z*"), request_id, status)
  end

  def to_s()
    if(is_error?())
      puts("ERROR :: #{@reason}")
      return
    end

    if(is_request?())
      if(@command_id == COMMAND_PING)
        return "COMMAND_PING  :: request_id = 0x%04x, data = %s" % [@request_id, @data]
      elsif(@command_id == COMMAND_SHELL)
        return "COMMAND_SHELL :: request_id = 0x%04x, name = %s" % [@request_id, @name]
      elsif(@command_id == COMMAND_EXEC)
        return "COMMAND_EXEC  :: request_id = 0x%04x, name = %s, command = %s" % [@request_id, @name, @command]
      else
        raise(DnscatException, "Unknown command_id: 0x%04x" % @command_id)
      end
    else
      if(@command_id == COMMAND_PING)
        return "COMMAND_PING  :: request_id = 0x%04x, data = %s" % [@request_id, @data]
      elsif(@command_id == COMMAND_SHELL)
        return "COMMAND_SHELL :: request_id = 0x%04x, session_id = 0x%04x" % [@request_id, @session_id]
      elsif(@command_id == COMMAND_EXEC)
        return "COMMAND_EXEC  :: request_id = 0x%04x, session_id = 0x%04x" % [@request_id, @session_id]
      else
        raise(DnscatException, "Unknown command_id: 0x%04x" % @command_id)
      end
    end
  end
end

### Test code
#requests = CommandPacket.create_ping_request(0x1111, "ping request") +
#  CommandPacket.create_shell_request(0x3333, "shell name") +
#  CommandPacket.create_exec_request(0x5555, "exec name", "exec command")
#
#responses = CommandPacket.create_ping_response(0x2222, "ping response") +
#  CommandPacket.create_shell_response(0x4444, 0x1234) +
#  CommandPacket.create_exec_response(0x6666, 0x4321)
#
#c = CommandPacketStream.new()
#c.feed(requests, true) do |packet|
#  puts(packet.to_s)
#end
#
#c.feed(responses, false) do |packet|
#  puts(packet.to_s)
#end
