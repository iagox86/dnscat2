##
# session.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'controller/packet'
require 'libs/commander'
require 'libs/dnscat_exception'
require 'libs/log'
require 'libs/swindow'

class Session
  @@isn = nil # nil = random

  attr_reader :id, :name, :options
  attr_reader :swindow

  # Session states
  STATE_NEW         = 0x00
  STATE_ESTABLISHED = 0x01
  STATE_KILLED      = 0xFF

  HANDLERS = {
    Packet::MESSAGE_TYPE_SYN => :_handle_syn,
    Packet::MESSAGE_TYPE_MSG => :_handle_msg,
    Packet::MESSAGE_TYPE_FIN => :_handle_fin,
  }

  def initialize(main_window)
    @state = STATE_NEW
    @their_seq = 0
    @my_seq    = @@isn.nil? ? rand(0xFFFF) : @@isn
    @options = 0

    @incoming_data = ''
    @outgoing_data = ''
    @name = 'unnamed'

    @main_window = main_window
  end

  def kill()
    if(@state != STATE_KILLED)
      @state = STATE_KILLED
    end
  end

  def _syn_valid?()
    return @state == STATE_NEW
  end

  def _msg_valid?()
    return @state == STATE_ESTABLISHED || @state == STATE_KILLED
  end

  def _fin_valid?()
    return @state == STATE_ESTABLISHED || @state == STATE_KILLED
  end

  def _next_outgoing(n)
    ret = @outgoing_data[0,n]
    return ret
  end

  def _ack_outgoing(n)
    # "n" is the current ACK value
    bytes_acked = (n - @my_seq)

    # Handle wraparounds properly
    if(bytes_acked < 0)
      bytes_acked += 0x10000
    end

    if(bytes_acked > 0)
      # TODO: Log data acknowledged
    end

    @outgoing_data = @outgoing_data[bytes_acked..-1]
    @my_seq = n
  end

  def _valid_ack?(ack)
    bytes_acked = (ack - @my_seq) & 0xFFFF
    return bytes_acked <= @outgoing_data.length
  end

  def queue_outgoing(data)
    @outgoing_data = @outgoing_data + data.force_encoding("ASCII-8BIT")
  end

  def to_s()
    return "id: 0x%04x, state: %d, their_seq: 0x%04x, my_seq: 0x%04x, incoming_data: %d bytes [%s], outgoing data: %d bytes [%s]" % [@id, @state, @their_seq, @my_seq, @incoming_data.length, @incoming_data, @outgoing_data.length, @outgoing_data]
  end

  def _handle_syn(packet)
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!syn_valid?())
      return nil
    end

    if(@state == STATE_KILLED)
      return Packet.create_fin(@options, {
        :session_id => @id,
        :reason => "Session was killed",
      })
    end

    # Save some of their options
    @their_seq = packet.body.seq
    @name      = packet.body.name
    @options   = packet.body.options
    @state     = STATE_ESTABLISHED

    # TODO: Somewhere in here, I need the concept of a 'parent' session
    @window = SWindow.new(@name, "%s %d>" % [@name, @id], @main_window, true)

    # TODO: Determine the type of session and register commands
    @window.on_input() do |data|
      @outgoing_data += data
    end


    return Packet.create_syn(0, {
      :session_id => @id,
      :seq        => @my_seq,
      :options    => 0, # TODO: I haven't paid much attention to what the server puts in its options field, should I?
    })
  end

  def _actual_msg_max_length(max_data_length)
    return max_data_length - (Packet.header_size(@options) + Packet::MsgBody.header_size(@options))
  end

  def _handle_msg(packet, max_length)
    if(!msg_valid?())
      kill()

      return Packet.create_fin(@options, {
        :session_id => @id,
        :reason     => "MSG received in invalid state",
      })
    end

    if(@state == STATE_KILLED)
      return Packet.create_fin(@options, {
        :session_id => @id,
        :reason => "Session was killed",
      })
    end

    # Validate the sequence number
    if(@their_seq != packet.body.seq)
      # Re-send the last packet
      old_data = next_outgoing(actual_msg_max_length(max_length))
      return Packet.create_msg(@options, {
        :session_id => @id,
        :data       => old_data,
        :seq        => @my_seq,
        :ack        => @their_seq,
      })
    end

    # Validate the acknowledgement number
    if(!valid_ack?(packet.body.ack))
      # Re-send the last packet
      old_data = next_outgoing(actual_msg_max_length(max_length))
      return Packet.create_msg(@options, {
        :session_id => @id,
        :data       => old_data,
        :seq        => @my_seq,
        :ack        => @their_seq,
      })
    end

    # Acknowledge the data that has been received so far
    # Note: this is where @my_seq is updated
    _ack_outgoing(packet.body.ack)

    # Write the incoming data to the session
    # TODO

    # Increment the expected sequence number
    @their_seq = (@their_seq + packet.body.data.length) & 0xFFFF;

    # Read the next piece of data
    new_data = next_outgoing(actual_msg_max_length(max_length))

    # Create a packet out of it
    packet = Packet.create_msg(@options, {
      :session_id => @id,
      :data       => new_data,
      :seq        => @my_seq,
      :ack        => @their_seq,
    })

    return packet
  end

  def _handle_fin(packet)
    # Ignore errant FINs - if we respond to a FIN with a FIN, it would cause a potential infinite loop
    if(!fin_valid?())
      return Packet.create_fin(@options, {
        :session_id => @id,
        :reason => "FIN not expected",
      })
    end

    SessionManager.kill_session(@id)

    return Packet.create_fin(@options, {
      :session_id => @id,
      :reason => "Bye!",
    })
  end

  def feed(data, max_length)
    packet = Packet.parse(data, @options)

    response_packet = send(HANDLERS[packet.type], packet)

    return response_packet.to_bytes()
  end
end
