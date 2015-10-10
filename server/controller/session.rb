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
require 'drivers/driver_process'
require 'libs/commander'
require 'libs/dnscat_exception'
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

    # TODO: Somewhere in here, I need the concept of a 'parent' session
    @settings = Settings.new()
    @window = SWindow.new(main_window, false, {:times_out => true})

    @settings.create("prompt", Settings::TYPE_NO_STRIP, "not set> ", "Change the prompt (if you want a space, use quotes; 'set prompt=\"a> \"'.") do |old_val, new_val|
      @window.prompt = new_val
    end

    @settings.create("name", Settings::TYPE_NO_STRIP, "(not set)", "Change the name of the window, and how it's displayed on the 'windows' list; this implicitly changes the prompt as well.") do |old_val, new_val|
      @window.name = new_val
      @settings.set("prompt", "%s %d> " % [new_val, @window.id])
    end

    @settings.create("history_size", Settings::TYPE_INTEGER, @window.history_size, "Change the number of lines to store in the window's history") do |old_val, new_val|
      @window.history_size = new_val
      @window.puts("history_size (session) => #{new_val}")
    end
  end

  def kill()
    @window.with({:to_ancestors=>true, :to_descendants=>true}) do
      if(@state != STATE_KILLED)
        @state = STATE_KILLED
        @window.puts("Session #{@window.id} has been killed")
      else
        @window.puts("Session #{@window.id} has been killed (again)")
      end
    end

    @window.close()
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
    return "id: 0x%04x [internal: %d], state: %d, their_seq: 0x%04x, my_seq: 0x%04x, incoming_data: %d bytes [%s], outgoing data: %d bytes [%s]" % [@id, @window.id, @state, @their_seq, @my_seq, @incoming_data.length, @incoming_data, @outgoing_data.length, @outgoing_data]
  end

  def Session._create_syn(session_id, my_sequence, options)
    return Packet.create_syn(0, {
      :session_id => session_id,
      :seq        => my_sequence,
      :options    => options, # TODO: I haven't paid much attention to what the server puts in its options field, should I?
    })
  end

  def _handle_syn(packet, max_length)
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!_syn_valid?())
      if(@their_seq == packet.body.seq && @options == packet.body.options)
        @window.puts("[WARNING] Duplicate SYN received!")
        return Session._create_syn(@id, @my_seq, 0)
      else
        raise(DnscatException, "Illegal SYN received")
      end
    end

    # Save some of their options
    @their_seq = packet.body.seq
    @options   = packet.body.options
    @state     = STATE_ESTABLISHED

    # TODO: We're going to need different driver types
    if((@options & Packet::OPT_COMMAND) == Packet::OPT_COMMAND)
      @driver = DriverCommand.new(@window, @settings)
    else
      process = @settings.get("process")
      if(process.nil?)
        @driver = DriverConsole.new(@window, @settings)
      else
        @driver = DriverProcess.new(@window, @settings, process)
      end
    end

    @settings.set("name", packet.body.name)

    if(Settings::GLOBAL.get("auto_attach"))
      @window.activate()
    end

    return Session._create_syn(@id, @my_seq, 0)
  end

  def _actual_msg_max_length(max_data_length)
    return max_data_length - (Packet.header_size(@options) + Packet::MsgBody.header_size(@options))
  end

  def _handle_msg(packet, max_length)
    if(!_msg_valid?())
      raise(DnscatException, "MSG received in invalid state!")
    end

    # We can send a FIN and close right away if the session is dead
    if(@state == STATE_KILLED)
      return Packet.create_fin(@options, {
        :session_id => @id,
        :reason => "The user killed the session!",
      })
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

  def _get_pcap_window()
    id = "pcap#{@window.id}"

    if(SWindow.exists?(id))
      return SWindow.get(id)
    end

    return SWindow.new(@window, false, {
      :id => id,
      :name => "dnscat2 protocol window for session #{@window.id}",
      :noinput => true,
    })
  end

  def feed(data, max_length)
    # Tell the window that we're still alive
    window.kick()

    packet = Packet.parse(data, @options)

    if(Settings::GLOBAL.get("packet_trace"))
      window = _get_pcap_window()
      window.puts("IN:  #{packet}")
    end

    begin
      response_packet = send(HANDLERS[packet.type], packet, max_length)
    rescue DnscatException => e
      @window.puts("Protocol exception occurred: %s" % e.to_s())
      @window.puts()
      @window.puts("If you think this might be a bug, please report with the")
      @window.puts("following stacktrace:")
      @window.puts(e.inspect)
      e.backtrace.each do |bt|
        @window.puts(bt)
      end

      @window.puts("Killing the session and responding with a FIN packet")
      kill()

      response_packet = Packet.create_fin(@options, {
        :session_id => @id,
        :reason => "An unhandled exception killed the session: %s" % e.to_s(),
      })
    end

    # If the program needs to ignore the packet, then it returns nil, and we
    # return a bunch of nothing
    if(response_packet.nil?)
      window.puts("OUT: <no data>")
      return ''
    end

    if(response_packet.to_bytes().length() > max_length)
      @window.puts("The session tried to return a packet that's too long!")
      @window.puts("#{response_packet}")
    end

    if(Settings::GLOBAL.get("packet_trace"))
      window = _get_pcap_window()
      window.puts("OUT: #{response_packet}")
    end

    return response_packet.to_bytes()
  end
end
