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

  def parse_header(data, is_request)
    # The length is already handled by command_packet_stream
    at_least?(data, 4) || raise(DnscatException, "Command packet is too short (header)")

    # (uint16_t) request_id
    @request_id, data = data.unpack("na*")

    if(is_request)
      # (uint16_t) command_id (request only)
      @command_id, data = data.unpack("na*")
    else
      # (uint16_t) status (response only)
      @status, data = data.unpack("na*")
    end

    return data
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

  def parse_body(data, is_request)
    if(@request_id.nil?)
      raise(Exception, "parse_header() has to be called before parse_body()")
    end

    if(@request_id == COMMAND_PING)
      return parse_ping(data, is_request)
    elsif(@request_id == COMMAND_SHELL)
      return parse_(data, is_request)
    elsif(@request_id == COMMAND_EXEC)
      return parse_ping(data, is_request)
    else
      raise(DnscatException, "Unknown command!")
    end
  end
end
