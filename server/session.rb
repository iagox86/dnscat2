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
require 'dnscat_exception'

class Session
  @@sessions = {}
  @@isn = nil # nil = random

  attr_reader :id, :state, :their_seq, :my_seq
  attr_reader :name

  # Session states
  STATE_NEW         = 0x00
  STATE_ESTABLISHED = 0x01

  def Session.debug_set_isn(n)
    Log.ERROR("Using debug code")
    @@isn = n
  end

  # Begin subscriber stuff (this should be in a mixin, but static stuff doesn't
  # really seem to work
  @@subscribers = []
  def Session.subscribe(cls)
    @@subscribers << cls
  end
  def Session.unsubscribe(cls)
    @@subscribers.delete(cls)
  end
  def Session.notify_subscribers(method, args)
    @@subscribers.each do |subscriber|
      if(subscriber.respond_to?(method))
         subscriber.method(method).call(*args)
      end
    end
  end
  # End subscriber stuff

  def initialize(id)
    @id = id
    @state = STATE_NEW
    @their_seq = 0
    @my_seq    = @@isn.nil? ? rand(0xFFFF) : @@isn

    @incoming_data = ''
    @outgoing_data = ''
    @name = ''

    Session.notify_subscribers(:session_created, [@id])
  end

  def Session.create_session(id)
    session = Session.new(id)
    @@sessions[id] = session
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
      raise(DnscatException, "Trying to set remote side's SEQ in the wrong state")
    end

    @their_seq = seq
  end

  def set_name(name)
    @name = name
  end

  def increment_their_seq(n)
    if(@state != STATE_ESTABLISHED)
      raise(DnscatException, "Trying to increment remote side's SEQ in the wrong state")
    end

    # Make sure we wrap their seq around after 0xFFFF, due to it being 16-bit
    @their_seq = (@their_seq + n) & 0xFFFF;
  end

  def set_established()
    if(@state != STATE_NEW)
      raise(DnscatException, "Trying to make a connection established from the wrong state")
    end

    @state = STATE_ESTABLISHED

    Session.notify_subscribers(:session_established, [@id])
  end

  def incoming?()
    return @incoming_data.length > 0
  end

  def queue_incoming(data)
    if(data.length > 0)
      Session.notify_subscribers(:session_data_received, [@id, data])
    end
  end

  def read_outgoing(n)
    ret = @outgoing_data[0,n]
    Session.notify_subscribers(:session_data_sent, [@id, ret])
    return ret
  end

  def ack_outgoing(n)
    # "n" is the current ACK value
    bytes_acked = (n - @my_seq)

    # Handle wraparounds properly
    if(bytes_acked < 0)
      bytes_acked += 0x10000
    end

    if(bytes_acked > 0)
      Session.notify_subscribers(:session_data_acknowledged, [@id, @outgoing_data[0..(bytes_acked-1)]])
    end

    puts("ACKing...")
    @outgoing_data = @outgoing_data[bytes_acked..-1]
    @my_seq = n
  end

  def valid_ack?(ack)
    bytes_acked = (ack - @my_seq) & 0xFFFF
    return bytes_acked <= @outgoing_data.length
  end

  def queue_outgoing(data)
    @outgoing_data = @outgoing_data + data
    puts("outgoing data: #{@outgoing_data}")
    Session.notify_subscribers(:session_data_queued, [@id, data])
  end

  def Session.exists?(id)
    return !@@sessions[id].nil?
  end

  def Session.find(id)
    return @@sessions[id]
  end

  def Session.destroy(id)
    # Notify subscribers before deleting it, in case they want to do something with
    # it first
    Session.notify_subscribers(:session_destroyed, [id])
    @@sessions.delete(id)
  end

  def Session.list()
    return @@sessions
  end

  def destroy()
    Session.destroy(@id)
  end

  def to_s()
    return "id: 0x%04x, state: %d, their_seq: 0x%04x, my_seq: 0x%04x, incoming_data: %d bytes [%s], outgoing data: %d bytes [%s]" % [@id, @state, @their_seq, @my_seq, @incoming_data.length, @incoming_data, @outgoing_data.length, @outgoing_data]
  end
end
