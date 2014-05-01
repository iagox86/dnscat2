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

  # These two methods are required for test.rb to work
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

    @killed = false

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

  def still_active?()
    if(@killed)
      return false
    end

    if(!@filename.nil?)
      if(@outgoing_data.length == 0)
        return false
      end
    end

    return true
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
    @outgoing_data = @outgoing_data + data
    notify_subscribers(:session_data_queued, [@id, data])
  end

  def to_s()
    return "id: 0x%04x, state: %d, their_seq: 0x%04x, my_seq: 0x%04x, incoming_data: %d bytes [%s], outgoing data: %d bytes [%s]" % [@id, @state, @their_seq, @my_seq, @incoming_data.length, @incoming_data, @outgoing_data.length, @outgoing_data]
  end

  def handle_syn(packet)
    puts(packet.to_s)
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!syn_valid?())
      notify_subscribers(:dnscat2_state_error, [@id, "SYN received in invalid state"])
      return nil
    end

    # Save some of their options
    @their_seq = packet.seq
    @name = packet.name
    @options = packet.options

    # Make sure options are sane
    if((@options & Packet::OPT_CHUNKED_DOWNLOAD) == Packet::OPT_CHUNKED_DOWNLOAD &&
       (@options & Packet::OPT_DOWNLOAD) == 0)
      notify_subscribers(:dnscat2_bad_options, ["OPT_CHUNKED_DOWNLOAD set without OPT_DOWNLOAD"])
      Log.ERROR("OPT_CHUNKED_DOWNLOAD set without OPT_DOWNLOAD")
      return Packet.create_fin(@id, "ERROR: OPT_CHUNKED_DOWNLOAD set without OPT_DOWNLOAD", @options)
    end

    # TODO: Allowing any arbitrary file is a security risk
    if(!packet.download.nil?)
      begin
        @filename = packet.download
        File.open(@filename, 'rb') do |f|
          queue_outgoing(f.read())
        end
      rescue Exception => e
        Log.ERROR("Client requested a bad file: #{packet.download}")
        Log.ERROR(e.inspect)

        return Packet.create_fin(@id, "ERROR: File couldn't be read: #{e.inspect}", @options)
      end
    end

    # Establish the session officially
    @state = STATE_ESTABLISHED
    notify_subscribers(:session_established, [@id])

    # Notify subscribers that the syn has come (TODO: I doubt we need this)
    notify_subscribers(:dnscat2_syn_received, [@id, @my_seq, packet.seq])

    return Packet.create_syn(@id, @my_seq, @options)
  end

  def handle_msg_normal(packet, max_length)
    # Validate the sequence number
    if(@their_seq != packet.seq)
      notify_subscribers(:dnscat2_msg_bad_seq, [@their_seq, packet.seq])

      # Re-send the last packet
      old_data = next_outgoing(max_length - Packet.msg_header_size(@options))
      return Packet.create_msg(@id, old_data, @options, {'seq'=>@my_seq,'ack'=>@their_seq})
    end

    # Validate the acknowledgement number
    if(!valid_ack?(packet.ack))
      notify_subscribers(:dnscat2_msg_bad_ack, [@my_seq, packet.ack])

      # Re-send the last packet
      old_data = next_outgoing(max_length - Packet.msg_header_size(@options))
      return Packet.create_msg(@id, old_data, @options, {'seq'=>@my_seq,'ack'=>@their_seq})
    end

    # Check if the session wants to close
    if(!still_active?())
      Log.WARNING("Session is finished, sending a FIN out")
      return Packet.create_fin(@id, "Session is gone", @options)
    end

    # Acknowledge the data that has been received so far
    # Note: this is where @my_seq is updated
    ack_outgoing(packet.ack)

    # Write the incoming data to the session
    # Increment the expected sequence number
    @their_seq = (@their_seq + packet.data.length) & 0xFFFF;

    # Let everybody know that data has arrived
    if(packet.data.length > 0)
      notify_subscribers(:session_data_received, [@id, packet.data])
    end

    # Read the next piece of data
    new_data = next_outgoing(max_length - Packet.msg_header_size(@options))

    # Create a packet out of it
    packet = Packet.create_msg(@id, new_data, @options, { 'seq' => @my_seq, 'ack' => @their_seq, })


    return packet

  end

  def handle_msg_chunked(packet, max_length)
    chunks = @outgoing_data.scan(/.{16}/m)

    if(chunks[packet.chunk].nil?)
      return Packet.create_fin(@id, "Chunk doesn't exist", @options)
    end

    chunk = chunks[packet.chunk]
    return Packet.create_msg(@id, chunk, @options, { 'chunk' => packet.chunk })
  end

  def handle_msg(packet, max_length)
    if(!msg_valid?())
      notify_subscribers(:dnscat2_state_error, [@id, "MSG received in invalid state; sending FIN"])

      # Kill the session as well - in case it exists
      SessionManager.kill_session(@id)

      return Packet.create_fin(@id, "MSG received in invalid state", @options)
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
      notify_subscribers(:dnscat2_state_error, [@id, "FIN received in invalid state"])
      return Packet.create_fin(@id, "FIN not expected", @options)
    end

    notify_subscribers(:dnscat2_fin, [@id, packet.reason])
    SessionManager.kill_session(@id)

    return Packet.create_fin(@id, "Bye!", @options)
  end
end


