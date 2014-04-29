##
# session.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
##

require 'log'
require 'dnscat_exception'
require 'subscribable'

class Session
  include Subscribable

  @@isn = nil # nil = random

  attr_reader :id, :state, :their_seq, :my_seq
  attr_reader :name
  attr_reader :options

  # Session states
  STATE_NEW         = 0x00
  STATE_ESTABLISHED = 0x01

  def Session.debug_set_isn(n)
    Log.FATAL("Using debug code")
    @@isn = n
  end
  def debug_set_seq(n)
    Log.FATAL("Using debug code")
    @my_seq = n
  end

  def initialize(id)
    @id = id
    @state = STATE_NEW
    @their_seq = 0
    @my_seq    = @@isn.nil? ? rand(0xFFFF) : @@isn
    @options = 0

    @incoming_data = ''
    @outgoing_data = ''
    @name = ''

    notify_subscribers(:session_created, [@id])
  end

  def syn_valid?()
    return @state == STATE_NEW
  end

  def msg_valid?()
    return @state == STATE_ESTABLISHED
  end

  def fin_valid?()
    return @state == STATE_ESTABLISHED
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

  def set_file(filename)
    @filename = filename
    File.open(filename) do |f|
      queue_outgoing(f.read())
    end
  end

  def still_active?()
    if(!@filename.nil?)
      if(@outgoing_data.length == 0)
        return false
      end
    end

    return true
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

    notify_subscribers(:session_established, [@id])
  end

  def incoming?()
    return @incoming_data.length > 0
  end

  def queue_incoming(data)
    if(data.length > 0)
      notify_subscribers(:session_data_received, [@id, data])
    end
  end

  def read_outgoing(n)
    ret = @outgoing_data[0,n]
    notify_subscribers(:session_data_sent, [@id, ret])
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
      notify_subscribers(:session_data_acknowledged, [@id, @outgoing_data[0..(bytes_acked-1)]])
    end

    @outgoing_data = @outgoing_data[bytes_acked..-1]
    @my_seq = n
  end

  def valid_ack?(ack)
    bytes_acked = (ack - @my_seq) & 0xFFFF
    return bytes_acked <= @outgoing_data.length
  end

  def queue_outgoing(data)
    @outgoing_data = @outgoing_data + data
    notify_subscribers(:session_data_queued, [@id, data])
  end

  def is_data_queued?()
    return @outgoing_data.length > 0
  end

#  def destroy()
#    SessionManager.destroy(@id)
#  end

  def to_s()
    return "id: 0x%04x, state: %d, their_seq: 0x%04x, my_seq: 0x%04x, incoming_data: %d bytes [%s], outgoing data: %d bytes [%s]" % [@id, @state, @their_seq, @my_seq, @incoming_data.length, @incoming_data, @outgoing_data.length, @outgoing_data]
  end

  def handle_syn(packet)
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!syn_valid?())
      notify_subscribers(:dnscat2_state_error, [@id, "SYN received in invalid state"])
      return nil
    end

    set_their_seq(packet.seq)
    set_name(packet.name)
    @options = packet.options

    # TODO: Allowing any arbitrary file is a security risk
    if(!packet.download.nil?)
      set_file(packet.download)
    end

    set_established()
    notify_subscribers(:dnscat2_syn_received, [@id, @my_seq, packet.seq])


    return Packet.create_syn(@id, @my_seq, @options)
  end

  def handle_msg(packet, max_length)
    if(!msg_valid?())
      notify_subscribers(:dnscat2_state_error, [@id, "MSG received in invalid state; sending FIN"])

      # Kill the session as well - in case it exists
      SessionManager.kill_session(@id)

      return Packet.create_fin(@id, @options)
    end

    # Validate the sequence number
    if(@their_seq != packet.seq)
      notify_subscribers(:dnscat2_msg_bad_seq, [@their_seq, packet.seq])

      # Re-send the last packet
      old_data = read_outgoing(max_length - Packet.msg_header_size(@options))
      return Packet.create_msg(@id, @my_seq, @their_seq, old_data, @options)
    end

    # Validate the acknowledgement number
    if(!valid_ack?(packet.ack))
      notify_subscribers(:dnscat2_msg_bad_ack, [@my_seq, packet.ack])

      # Re-send the last packet
      old_data = read_outgoing(max_length - Packet.msg_header_size(@options))
      return Packet.create_msg(@id, @my_seq, @their_seq, old_data, @options)
    end

    # Check if the session wants to close
    if(!still_active?())
      Log.WARNING("Session is finished, sending a FIN out")
      return Packet.create_fin(@id, @options)
    end

    # Acknowledge the data that has been received so far
    # Note: this is where @my_seq is updated
    ack_outgoing(packet.ack)

    # Write the incoming data to the session
    queue_incoming(packet.data)

    # Increment the expected sequence number
    increment_their_seq(packet.data.length)

    new_data = read_outgoing(max_length - Packet.msg_header_size(@options))
    notify_subscribers(:dnscat2_msg, [packet.data, new_data])

    # Build the new packet
    return Packet.create_msg(@id, @my_seq, @their_seq, new_data, @options)
  end

  def handle_fin(packet)
    # Ignore errant FINs - if we respond to a FIN with a FIN, it would cause a potential infinite loop
    if(!fin_valid?())
      notify_subscribers(:dnscat2_state_error, [@id, "FIN received in invalid state"])
      return Packet.create_fin(@id, @options)
    end

    notify_subscribers(:dnscat2_fin, [@id])
    SessionManager.kill_session(@id)

    return Packet.create_fin(@id, @options)
  end
end


