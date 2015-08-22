##
# session.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'libs/dnscat_exception'
require 'libs/log'
require 'libs/packet'

class Session
  @@isn = nil # nil = random

  attr_reader :id, :state, :their_seq, :my_seq
  attr_reader :name
  attr_reader :options
  attr_reader :is_command

  # Session states
  STATE_NEW         = 0x00
  STATE_ESTABLISHED = 0x01
  STATE_KILLED      = 0xFF

  def initialize()
    @state = STATE_NEW
    @their_seq = 0
    @my_seq    = @@isn.nil? ? rand(0xFFFF) : @@isn
    @options = 0
    @is_command = false

    @incoming_data = ''
    @outgoing_data = ''
    @name = ''
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

    # Make sure options are sane
    if((@options & Packet::OPT_CHUNKED_DOWNLOAD) == Packet::OPT_CHUNKED_DOWNLOAD &&
       (@options & Packet::OPT_DOWNLOAD) == 0)
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

    return Packet.create_syn(0, {
      :session_id => @id,
      :seq        => @my_seq,
      :options    => 0, # TODO: I haven't paid much attention to what the server puts in its options field
    })
  end

  def _actual_msg_max_length(max_data_length)
    return max_data_length - (Packet.header_size(@options) + Packet::MsgBody.header_size(@options))
  end

  def _handle_msg_normal(packet, max_length)
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
    ack_outgoing(packet.body.ack)

    # Write the incoming data to the session
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

  def _handle_msg(packet, max_length)
    if(!msg_valid?())
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

  end
end
