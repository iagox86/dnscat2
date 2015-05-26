##
# session.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'dnscat_exception'
require 'log'
require 'packet'
require 'subscribable'

class Session
  include Subscribable

  @@isn = nil # nil = random

  attr_reader :id, :state, :their_seq, :my_seq
  attr_reader :name
  attr_reader :options
  attr_reader :is_command

  # Session states
  STATE_NEW         = 0x00
  STATE_ESTABLISHED = 0x01
  STATE_KILLED      = 0xFF

  # These two methods are required for test.rb to work
  def Session.debug_set_isn(n)
    Log.FATAL(nil, "Using debug code")
    @@isn = n
  end
  def debug_set_seq(n)
    Log.FATAL(@id, "Using debug code")
    @my_seq = n
  end

  def initialize(id)
    @id = id
    @state = STATE_NEW
    @their_seq = 0
    @my_seq    = @@isn.nil? ? rand(0xFFFF) : @@isn
    @options = 0
    @is_command = false

    @incoming_data = ''
    @outgoing_data = ''
    @name = ''

    initialize_subscribables()
    notify_subscribers(:session_created, [@id])
  end

  def kill()
    if(@state != STATE_KILLED)
      Log.WARNING(@id, "Session killed")

      @state = STATE_KILLED
    end
  end

  def syn_valid?()
    return @state == STATE_NEW
  end

  def msg_valid?()
    return @state == STATE_ESTABLISHED || @state == STATE_KILLED
  end

  def fin_valid?()
    return @state == STATE_ESTABLISHED || @state == STATE_KILLED
  end

  def next_outgoing(n)
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
    @outgoing_data = @outgoing_data + data.force_encoding("ASCII-8BIT")
    notify_subscribers(:session_data_queued, [@id, data])
  end

  def to_s()
    return "id: 0x%04x, state: %d, their_seq: 0x%04x, my_seq: 0x%04x, incoming_data: %d bytes [%s], outgoing data: %d bytes [%s]" % [@id, @state, @their_seq, @my_seq, @incoming_data.length, @incoming_data, @outgoing_data.length, @outgoing_data]
  end

  def handle_syn(packet)
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!syn_valid?())
      notify_subscribers(:dnscat2_session_error, [@id, "SYN received in invalid state"])
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

    # Make sure options are sane
    if((@options & Packet::OPT_CHUNKED_DOWNLOAD) == Packet::OPT_CHUNKED_DOWNLOAD &&
       (@options & Packet::OPT_DOWNLOAD) == 0)
      notify_subscribers(:dnscat2_session_error, [@id, "Error in client options: OPT_CHUNKED_DOWNLOAD set without OPT_DOWNLOAD"])
      return Packet.create_fin(@options, {
        :session_id => @id,
        :reason     => "ERROR: OPT_CHUNKED_DOWNLOAD set without OPT_DOWNLOAD",
      })
    end

    if((@options & Packet::OPT_COMMAND) == Packet::OPT_COMMAND)
      @is_command = true
    end

    # TODO: Allowing any arbitrary file is a security risk
    if(!packet.body.download.nil?)
      begin
        @filename = packet.body.download
        File.open(@filename, 'rb') do |f|
          queue_outgoing(f.read())
        end
      rescue Exception => e
        Log.ERROR(@id, "Client requested a bad file: #{packet.body.download}")
        Log.ERROR(@id, e.to_s())

        return Packet.create_fin(@options, {
          :session_id => @id,
          :reason     => "ERROR: File couldn't be read: #{e.inspect}",
        })
      end
    end

    # Establish the session officially
    @state = STATE_ESTABLISHED
    notify_subscribers(:session_established, [@id])

    # Notify subscribers that the syn has come (TODO: I doubt we need this)
    notify_subscribers(:dnscat2_syn_received, [@id, @my_seq, packet.body.seq])

    return Packet.create_syn(0, {
      :session_id => @id,
      :seq        => @my_seq,
      :options    => 0, # TODO: I haven't paid much attention to what the server puts in its options field
    })
  end

  def actual_msg_max_length(max_data_length)
    return max_data_length - (Packet.header_size(@options) + Packet::MsgBody.header_size(@options))
  end

  def handle_msg_normal(packet, max_length)
    # Validate the sequence number
    if(@their_seq != packet.body.seq)
      notify_subscribers(:dnscat2_session_error, [@id, "Bad sequence number on incoming packet: expected 0x%04x, received 0x%04x" % [@their_seq, packet.body.seq]])

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
      notify_subscribers(:dnscat2_session_error, [@id, "Bad acknowledgement number: expected 0x%04x, received 0x%04x" % [@my_seq, packet.body.ack]])

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
    ack_outgoing(packet.body.ack)

    # Write the incoming data to the session
    # Increment the expected sequence number
    @their_seq = (@their_seq + packet.body.data.length) & 0xFFFF;

    # Let everybody know that data has arrived
    if(packet.body.data.length > 0)
      notify_subscribers(:session_data_received, [@id, packet.body.data])
    end

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

  def handle_msg_chunked(packet, max_length)
    chunks = @outgoing_data.scan(/.{16}/m)

    if(chunks[packet.body.chunk].nil?)
      return Packet.create_fin(@options, {
        :session_id => @id,
        :reason => "Chunk doesn't exist!",
      })
    end

    chunk = chunks[packet.body.chunk]
    return Packet.create_msg(@options, {
      :session_id => @id,
      :chunk      => chunk,
    })
  end

  def handle_msg(packet, max_length)
    if(!msg_valid?())
      notify_subscribers(:dnscat2_session_error, [@id, "MSG received in invalid state; sending FIN"])

      # Kill the session as well - in case it exists
      SessionManager.kill_session(@id)

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

    if((@options & Packet::OPT_CHUNKED_DOWNLOAD) == Packet::OPT_CHUNKED_DOWNLOAD)
      return handle_msg_chunked(packet, max_length)
    else
      return handle_msg_normal(packet, max_length)
    end

    # TODO: Was this necessary?
    #notify_subscribers(:dnscat2_msg, [packet.data, new_data])
  end

  def handle_fin(packet)
    # Ignore errant FINs - if we respond to a FIN with a FIN, it would cause a potential infinite loop
    if(!fin_valid?())
      notify_subscribers(:dnscat2_session_error, [@id, "FIN received in invalid state"])

      return Packet.create_fin(@options, {
        :session_id => @id,
        :reason => "FIN not expected",
      })
    end

    notify_subscribers(:dnscat2_fin, [@id, packet.body.reason])
    SessionManager.kill_session(@id)

    return Packet.create_fin(@options, {
      :session_id => @id,
      :reason => "Bye!",
    })
  end
end


