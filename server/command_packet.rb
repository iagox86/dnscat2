##
# command_packet_stream.rb
# Created May, 2014
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'dnscat_exception'

require 'command_packet_stream'

class CommandPacket
  COMMAND_PING     = 0x0000
  COMMAND_SHELL    = 0x0001
  COMMAND_EXEC     = 0x0002
  COMMAND_DOWNLOAD = 0x0003
  COMMAND_UPLOAD   = 0x0004
  COMMAND_SHUTDOWN = 0x0005
  COMMAND_ERROR    = 0xFFFF

  attr_reader :request_id, :command_id # header
  attr_reader :data # ping
  attr_reader :name, :session_id # shell
  attr_reader :command # command
  attr_reader :filename, :data # download
  attr_reader :filename, :data # upload

  attr_reader :status, :reason # errors

  def at_least?(data, needed)
    return (data.length >= needed)
  end

  def is_error?()
    return @command_id == COMMAND_ERROR
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

  def parse_download(data, is_request)
    if(is_request)
      if(data.index("\0").nil?)
        raise(DnscatException, "Download packet request doesn't have a NUL byte after name")
      end
      @filename, data = data.unpack("Z*a*")

      if(data.length > 0)
        raise(DnscatException, "Download request packet has extra data on the end")
      end
    else
      @data = data.unpack("a*").pop
    end
  end

  def parse_upload(data, is_request)
    if(is_request)
      if(data.index("\0").nil?)
        raise(DnscatException, "Upload packet request doesn't have a NUL byte after name")
      end
      @filename, @data = data.unpack("Z*a*")
    else
      if(data.length > 0)
        raise(DnscatException, "Upload packet response has extra data on the end")
      end
    end
  end

  def parse_shutdown(data, is_request)
    if(is_request)
      if(data.length > 0)
        raise(DnscatException, "Shutdown packet response has extra data on the end")
      end
    else
      if(data.length > 0)
        raise(DnscatException, "Shutdown packet response has extra data on the end")
      end
    end
  end

  def parse_error(data, is_request)
    @status, data = data.unpack("na*")

    if(data.index("\0").nil?)
      raise(DnscatException, "Error packet doesn't have a NUL byte after name")
    end

    @reason, data = data.unpack("Z*a*")

    if(data.length > 0)
      raise(DnscatException, "Error packet has extra data on the end")
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

    if(@command_id == COMMAND_PING)
      parse_ping(data, is_request)
    elsif(@command_id == COMMAND_SHELL)
      parse_shell(data, is_request)
    elsif(@command_id == COMMAND_EXEC)
      parse_exec(data, is_request)
    elsif(@command_id == COMMAND_DOWNLOAD)
      parse_download(data, is_request)
    elsif(@command_id == COMMAND_UPLOAD)
      parse_upload(data, is_request)
    elsif(@command_id == COMMAND_SHUTDOWN)
      parse_shutdown(data, is_request)
    elsif(@command_id == COMMAND_ERROR)
      parse_error(data, is_request)
    else
      raise(DnscatException, "Unknown command: 0x%04x" % @command_id)
    end
  end

  def CommandPacket.add_header(packet, request_id, command_id)
    packet = [request_id, command_id, packet].pack("nna*")

    return [packet.length, packet].pack('Na*')
  end

  def CommandPacket.create_ping_request(request_id, data)
    return CommandPacket.add_header([data].pack('Z*'), request_id, COMMAND_PING)
  end
  def CommandPacket.create_ping_response(request_id, data)
    return CommandPacket.add_header([data].pack('Z*'), request_id, COMMAND_PING)
  end

  def CommandPacket.create_shell_request(request_id, name)
    return CommandPacket.add_header([name].pack('Z*'), request_id, COMMAND_SHELL)
  end
  def CommandPacket.create_shell_response(request_id, session_id)
    return CommandPacket.add_header([session_id].pack('n'), request_id, COMMAND_SHELL)
  end

  def CommandPacket.create_exec_request(request_id, name, command)
    return CommandPacket.add_header([name, command].pack('Z*Z*'), request_id, COMMAND_EXEC)
  end
  def CommandPacket.create_exec_response(request_id, session_id)
    return CommandPacket.add_header([session_id].pack('n'), request_id, COMMAND_EXEC)
  end

  def CommandPacket.create_download_request(request_id, filename)
    return CommandPacket.add_header([filename].pack('Z*'), request_id, COMMAND_DOWNLOAD)
  end
  def CommandPacket.create_download_response(request_id, data)
    return CommandPacket.add_header([data].pack('a*'), request_id, COMMAND_DOWNLOAD)
  end

  def CommandPacket.create_upload_request(request_id, filename, data)
    return CommandPacket.add_header([filename, data].pack('Z*a*'), request_id, COMMAND_UPLOAD)
  end
  def CommandPacket.create_upload_response(request_id)
    return CommandPacket.add_header('', request_id, COMMAND_UPLOAD)
  end

  def CommandPacket.create_shutdown_request(request_id)
    return CommandPacket.add_header('', request_id, COMMAND_SHUTDOWN)
  end
  def CommandPacket.create_shutdown_response(request_id)
    return CommandPacket.add_header('', request_id, COMMAND_SHUTDOWN)
  end

  def CommandPacket.create_error(request_id, status, reason)
    return CommandPacket.add_header([status, reason].pack("nZ*"), request_id)
  end

  def to_s()
    if(is_request?())
      if(@command_id == COMMAND_PING)
        return "COMMAND_PING      :: request_id = 0x%04x, data = %s" % [@request_id, @data]
      elsif(@command_id == COMMAND_SHELL)
        return "COMMAND_SHELL     :: request_id = 0x%04x, name = %s" % [@request_id, @name]
      elsif(@command_id == COMMAND_EXEC)
        return "COMMAND_EXEC      :: request_id = 0x%04x, name = %s, command = %s" % [@request_id, @name, @command]
      elsif(@command_id == COMMAND_DOWNLOAD)
        return "COMMAND_DOWNLOAD  :: request_id = 0x%04x, filename = %s" % [@request_id, @filename]
      elsif(@command_id == COMMAND_UPLOAD)
        return "COMMAND_UPLOAD    :: request_id = 0x%04x, filename = %s, data = 0x%x bytes" % [@request_id, @filename, @data.length]
      elsif(@command_id == COMMAND_ERROR)
        return "COMMAND_ERROR     :: request_id = 0x%04x, status = 0x%04x, reason = %s" % [@request_id, @status, @reason]
      else
        raise(DnscatException, "Unknown command_id: 0x%04x" % @command_id)
      end
    else
      if(@command_id == COMMAND_PING)
        return "COMMAND_PING     :: request_id = 0x%04x, data = %s" % [@request_id, @data]
      elsif(@command_id == COMMAND_SHELL)
        return "COMMAND_SHELL    :: request_id = 0x%04x, session_id = 0x%04x" % [@request_id, @session_id]
      elsif(@command_id == COMMAND_EXEC)
        return "COMMAND_EXEC     :: request_id = 0x%04x, session_id = 0x%04x" % [@request_id, @session_id]
      elsif(@command_id == COMMAND_DOWNLOAD)
        return "COMMAND_DOWNLOAD :: request_id = 0x%04x, data = 0x%x bytes" % [@request_id, @data.length]
      elsif(@command_id == COMMAND_UPLOAD)
        return "COMMAND_UPLOAD   :: request_id = 0x%04x" % [@request_id]
      elsif(@command_id == COMMAND_SHUTDOWN)
        return "COMMAND_SHUTDOWN :: request_id = 0x%04x" % [@request_id]
      elsif(@command_id == COMMAND_ERROR)
        return "COMMAND_ERROR    :: request_id = 0x%04x, status = 0x%04x, reason = %s" % [@request_id, @status, @reason]
      else
        raise(DnscatException, "Unknown command_id: 0x%04x" % @command_id)
      end
    end
  end
end
