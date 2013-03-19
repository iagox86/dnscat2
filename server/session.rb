##
# session.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# Handles dnscat2 sessions. Uses a global list of sessions (@@sessions), so
# they aren't bound to any particular instance of this class.
##

require 'log'

class Session
  @@sessions = {}
  @@isn = nil # nil = random

  attr_reader :id, :state, :their_seq, :my_seq

  # Session states
  STATE_NEW         = 0x00
  STATE_ESTABLISHED = 0x01

  def Session.debug_set_isn(n)
    Log.log("WARNING", "Using debug code")
    @@isn = n
  end

  def initialize(id)
    @id = id
    @state = STATE_NEW
    @their_seq = 0
    @my_seq    = @@isn.nil? ? rand(0xFFFF) : @@isn

    @incoming_data = ''
    @outgoing_data = ''
  end

  def syn_valid?()
    return @state == STATE_NEW
  end

  def msg_valid?()
    return @state != STATE_NEW
  end

  def fin_valid?()
    return @state != STATE_NEW
  end

  def set_their_seq(seq)
    # This can only be done in the NEW state
    if(@state != STATE_NEW)
      raise(IOError, "Trying to set remote side's SEQ in the wrong state")
    end

    @their_seq = seq
  end

  def increment_their_seq(n)
    if(@state != STATE_ESTABLISHED)
      raise(IOError, "Trying to increment remote side's SEQ in the wrong state")
    end
    @their_seq += n
  end

  def set_established()
    if(@state != STATE_NEW)
      raise(IOError, "Trying to make a connection established from the wrong state")
    end

    @state = STATE_ESTABLISHED
  end

  def read_incoming(n = nil)
    if(n.nil? || n < @incoming_data.length)
      ret = @incoming_data
      @incoming_data = ''
      return ret
    else
      ret = @incoming_data[0,n]
      @incoming_data = @incoming_data[n..-1]
    end
  end

  def queue_incoming(data)
    @incoming_data = @incoming_data + data
  end

  def read_outgoing(n = nil)
    if(n.nil? || n < @outgoing_data.length)
      ret = @outgoing_data
      return ret
    else
      ret = @outgoing_data[0,n]
    end
  end

  # TODO: Handle overflows
  def ack_outgoing(n)
    bytes_acked = (n - @my_seq)
    Log.log(id, "ACKing #{bytes_acked} bytes")
    @outgoing_data = @outgoing_data[bytes_acked..-1]
    @my_seq = n
  end

  def valid_ack?(ack)
    return (ack >= @my_seq && ack <= @my_seq + @outgoing_data.length)
  end

  def queue_outgoing(data)
    @outgoing_data = @outgoing_data + data
  end

  def Session.find(id)
    # Get or create the session
    if(@@sessions[id].nil?)
      Log.log(id, "Creating new session")
      @@sessions[id] = Session.new(id)
    end

    return @@sessions[id]
  end

  def Session.destroy(id)
    @@sessions.delete(id)
  end

  def destroy()
    Session.destroy(@id)
  end
end
