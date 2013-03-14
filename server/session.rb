
class Session
  @@sessions = {}

  attr_reader :id, :state, :their_seq, :my_seq

  # Session states
  STATE_NEW         = 0x00
  STATE_ESTABLISHED = 0x01

  def initialize(id)
    @id = id
    @state = STATE_NEW
    @their_seq = 0 # TODO: Initialize based on their SYN packet
    @my_seq    = 0 # TODO: Randomize

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

  def ack_outgoing(n)
    @outgoing_data = @outgoing_data[n..-1]
  end

  def queue_outgoing(data)
    @outgoing_data = @outgoing_data + data
  end

  def Session.find(id)
    # Get or create the session
    if(@@sessions[id].nil?)
      puts("[[#{id}]] :: create")
      @@sessions[id] = Session.new(id)
    else
      puts("[[#{id}]] :: found")
    end

    return @@sessions[id]
  end

  def Session.destroy(id)
    @@sessions.delete(id)
  end

  def destroy()
    Sessions.destroy(@id)
  end
end
