
class Session
  @@sessions = {}

  attr_reader :id, :state, :their_seq, :my_seq

  # Session states
  STATE_NEW      = 0x00
  STATE_ACTIVE   = 0x01

  def initialize(id)
    @id = id
    @state = STATE_NEW
    @their_seq = 0 # TODO: Initialize based on their SYN packet
    @my_seq    = 0 # TODO: Randomize
  end

  def is_syn_valid()
    return @state == STATE_NEW
  end

  def is_msg_valid()
    return @state != STATE_NEW
  end

  def is_fin_valid()
    return @state != STATE_NEW
  end

  def Session.find(id)
    # Get or create the session
    session = @@sessions[id]

    if(session.nil?)
      puts("[[#{id}]] :: create")
      @@sessions[id] = Session.new(id)
    end

    return session
  end

  def Session.destroy(id)
    @@sessions.delete(id)
  end

  def destroy()
    Sessions.destroy(@id)
  end
end
