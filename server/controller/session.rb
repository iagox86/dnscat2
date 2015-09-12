##
# session.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'controller/packet'
require 'drivers/driver_command'
require 'drivers/driver_console'
require 'libs/commander'
require 'libs/dnscat_exception'
require 'libs/log'
require 'libs/swindow'

class Session
  @@isn = nil # nil = random

  attr_reader :id, :name, :options, :state
  attr_reader :window

  # Session states
  STATE_NEW         = 0x00
  STATE_ESTABLISHED = 0x01
  STATE_KILLED      = 0xFF

  HANDLERS = {
    Packet::MESSAGE_TYPE_SYN => :_handle_syn,
    Packet::MESSAGE_TYPE_MSG => :_handle_msg,
    Packet::MESSAGE_TYPE_FIN => :_handle_fin,
  }

  def initialize(id, main_window)
    @state = STATE_NEW
    @their_seq = 0
    @my_seq    = @@isn.nil? ? rand(0xFFFF) : @@isn
    @options = 0

    @id = id
    @incoming_data = ''
    @outgoing_data = ''
    @name = 'unnamed'

    # TODO: Somewhere in here, I need the concept of a 'parent' session
    @settings = Settings.new()
    @window = SWindow.new(@name, "", main_window, false)

    @settings.create("prompt", Settings::TYPE_NO_STRIP, "", Proc.new() do |old_val, new_val|
      # We don't have any callbacks
      @window.prompt = new_val
    end)
  end

  def activate()
    @window.activate()
  end

  def deactivate()
    @window.deactivate()
  end

  def kill()
    if(@state != STATE_KILLED)
      @state = STATE_KILLED
      @window.puts_ex("Session #{@id} has been killed")
    else
      @window.puts_ex("Session #{@id} has been killed (again)")
    end
    deactivate()
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
    ret = @outgoing_data[0,n-1]
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

  def _handle_syn(packet, max_length)
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!_syn_valid?())
      raise(DnscatException, "SYN received in invalid state")
    end

    # Save some of their options
    @their_seq = packet.body.seq
    @name      = packet.body.name
    @options   = packet.body.options
    @state     = STATE_ESTABLISHED

    @window.puts_ex("New session established: %d" % @id, true, true)


    # TODO: We're going to need different driver types
    if((@options & Packet::OPT_COMMAND) == Packet::OPT_COMMAND)
      @driver = DriverCommand.new(@window, @settings)
    else
      @driver = DriverConsole.new(@window, @settings)
    end

    @settings.set("prompt", "%s %d> " % [@name, @id])

    # TODO: Check if auto_attach is set
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
    if(!_msg_valid?())
      raise(DnscatException, "MSG received in invalid state!")
    end

    # Validate the sequence number
    if(@their_seq != packet.body.seq)
      # Re-send the last packet
      old_data = _next_outgoing(_actual_msg_max_length(max_length))

      return Packet.create_msg(@options, {
        :session_id => @id,
        :data       => old_data,
        :seq        => @my_seq,
        :ack        => @their_seq,
      })
    end

    # Validate the acknowledgement number
    if(!_valid_ack?(packet.body.ack))
      # Re-send the last packet
      old_data = _next_outgoing(_actual_msg_max_length(max_length))

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
    @outgoing_data += @driver.feed(packet.body.data)

    # Increment the expected sequence number
    @their_seq = (@their_seq + packet.body.data.length) & 0xFFFF;

    # Read the next piece of data
    new_data = _next_outgoing(_actual_msg_max_length(max_length))

    # Create a packet out of it
    packet = Packet.create_msg(@options, {
      :session_id => @id,
      :data       => new_data,
      :seq        => @my_seq,
      :ack        => @their_seq,
    })

    return packet
  end

  def _handle_fin(packet, max_length)
    # Ignore errant FINs - if we respond to a FIN with a FIN, it would cause a potential infinite loop
    if(!_fin_valid?())
      raise(DnscatException, "FIN received in invalid state")
    end

    # End the session
    kill()

    return Packet.create_fin(@options, {
      :session_id => @id,
      :reason => "Bye!",
    })
  end

  def feed(data, max_length)
    packet = Packet.parse(data, @options)

    begin
      response_packet = send(HANDLERS[packet.type], packet, max_length)
    rescue DnscatException => e
      @window.puts("Protocol exception occurred: %s" % e.to_s())
      @window.puts("Protocol exception caught in dnscat DNS module (unable to determine session at this point to close it):")
      @window.puts(e.inspect)
      e.backtrace.each do |bt|
        @window.puts(bt)
      end
      kill()

      response_packet = Packet.create_fin(@options, {
        :session_id => @id,
        :reason => "An unhandled exception killed the session: %s" % e.to_s(),
      })
    end

    if(response_packet.to_bytes().length() > max_length)
      @window.puts("The session tried to return a packet that's too long!")
      @window.puts("#{response_packet}")
    end

    return response_packet.to_bytes()
  end
end
