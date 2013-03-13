
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
    if(@state != STATE_NEW)
      raise(IOError, "Trying to set remote side's SEQ in the wrong state")
    end

    @their_seq = seq
  end

  def set_established()
    if(@state != STATE_NEW)
      raise(IOError, "Trying to make a connection established from the wrong state")
    end

    @state = STATE_ESTABLISHED
  end

  def Session.find(id)
    # Get or create the session
    if(@@sessions[id].nil?)
      puts("[[#{id}]] :: create")
      @@sessions[id] = Session.new(id)
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
