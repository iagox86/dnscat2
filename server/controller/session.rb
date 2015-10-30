##
# session.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'controller/encryptor'
require 'controller/packet'
require 'drivers/driver_command'
require 'drivers/driver_console'
require 'drivers/driver_process'
require 'libs/commander'
require 'libs/dnscat_exception'
require 'libs/swindow'

class Session
  class SessionKiller < StandardError
  end

  @@isn = nil # nil = random

  attr_reader :id, :name, :options, :state
  attr_reader :window

  # The session hasn't seen a SYN packet yet (but may have done some encryption negotiation)
  STATE_NEW           = 0x00

  # After receiving a SYN
  STATE_ESTABLISHED   = 0x01

  # After being manually killed
  STATE_KILLED        = 0xFF

  HANDLERS = {
    Packet::MESSAGE_TYPE_SYN => :_handle_syn,
    Packet::MESSAGE_TYPE_MSG => :_handle_msg,
    Packet::MESSAGE_TYPE_FIN => :_handle_fin,
    Packet::MESSAGE_TYPE_ENC => :_handle_enc,
  }

  def initialize(id, main_window)
    @state = STATE_NEW
    @their_seq = 0
    @my_seq    = @@isn.nil? ? rand(0xFFFF) : @@isn
    @options = 0

    @id = id
    @incoming_data = ''
    @outgoing_data = ''

    @encryptor = nil

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

  def _handle_syn(packet, max_length)
    options = 0

    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(@state != STATE_NEW)
      raise(DnscatException, "Duplicate SYN received!")
    end

    # Save some of their options
    @their_seq = packet.body.seq
    @options   = packet.body.options

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

    if((@options & Packet::OPT_NAME) == Packet::OPT_NAME)
      @settings.set("name", packet.body.name)
    else
      @settings.set("name", "unnamed")
    end

    if(Settings::GLOBAL.get("auto_attach"))
      @window.activate()
    end

    # Feed the auto_command into the window, as if it was user input
    if(auto_command = Settings::GLOBAL.get("auto_command"))
      auto_command.split(";").each do |command|
        command = command.strip()
        window.fake_input(command)
      end
    end

    # Move states (this has to come after the encryption code, otherwise this packet is accidentally encrypted)
    @state = STATE_ESTABLISHED

    return Packet.create_syn(options, {
      :session_id => @id,
      :seq        => @my_seq
    })
  end

  def _actual_msg_max_length(max_data_length)
    return max_data_length - (Packet.header_size(@options) + Packet::MsgBody.header_size(@options))
  end

  def _handle_msg(packet, max_length)
    if(@state != STATE_ESTABLISHED)
      raise(DnscatException, "MSG received in invalid state!")
    end

    # Validate the sequence number
    if(@their_seq != packet.body.seq)
      @window.puts("Client sent a bad sequence number (expected #{@their_seq}, received #{packet.body.seq}); re-sending")

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
    raise(Session::SessionKiller, "Received FIN! Bye!")
  end

  def _handle_enc(packet, max_length)
    # TODO: Allow re-keying
    if(@state != STATE_NEW)
      raise(DnscatException, "Received an ENC packet in an invalid state (and re-negotiation isn't done yet!)")
    end

    params = {
      :session_id => @id,
      :subtype    => packet.body.subtype,
      :flags      => 0,
    }

    if(packet.body.subtype == Packet::EncBody::SUBTYPE_INIT)
      if(!@encryptor)
        @encryptor = Encryptor.new(Settings::GLOBAL.get('secret'))
      end

      @encryptor.set_their_public_key(packet.body.public_key_x, packet.body.public_key_y)

      @window.puts("Generated cryptographic values:")
      @window.puts(@encryptor)

      params[:public_key_x] = @encryptor.my_public_key_x()
      params[:public_key_y] = @encryptor.my_public_key_y()

      @window.with({:to_ancestors => true}) do
        @window.puts()
        @window.puts("Encrypted session established! For added security, please verify the client also displays this string:")
        @window.puts()
        @window.puts(@encryptor.get_sas())
        @window.puts()
      end
    elsif(packet.body.subtype == Packet::EncBody::SUBTYPE_AUTH)
      # Make sure we actually have an encryptor set
      if(!@encryptor)
        raise(DnscatException, "ENC_SUBTYPE_AUTH packet received before ENC_SUBTYPE_INIT")
      end

      # Check their authenticator
      begin
        @encryptor.set_their_authenticator(packet.body.authenticator)
      rescue Encryptor::Error
        raise(Session::SessionKiller, "Invalid authenticator (pre-shared secret)")
      end

      @window.puts("Encrypted session validated with the pre-shared secret!")

      params[:authenticator] = @encryptor.my_authenticator
    else
      raise(DnscatException, "Don't know how to parse encryption subtype in: #{packet}")
    end

    return Packet.create_enc(params)
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

  def _check_crypto_options(packet)
    # Don't enforce encryption on ENC|INIT packets
    if(packet.type == Packet::MESSAGE_TYPE_ENC && packet.body.subtype == Packet::EncBody::SUBTYPE_INIT)
      return
    end

    if(Settings::GLOBAL.get('security') == 'open')
      return
    end

    if(!@encryptor.ready?())
      @window.with({:to_ancestors => true}) do
        @window.puts("Client attempted to connect with encryption disabled!")
        @window.puts("If this was intentional, you can make encryption optional with 'set security=open'")
      end

      raise(Session::SessionKiller, "This server requires an encrypted connection!")
    end

    # Don't enforce authentication on AUTH packets
    if(packet.type == Packet::MESSAGE_TYPE_ENC && packet.body.subtype == Packet::EncBody::SUBTYPE_AUTH)
      return
    end

    if(Settings::GLOBAL.get('security') == 'encrypted')
      return
    end

    if(!@encryptor.authenticated?())
      @window.with({:to_ancestors => true}) do
        @window.puts("Client attempted to connect without a pre-shared secret!")
        @window.puts("If this was intentional, you can make authentication optional with 'set security=encrypted'")
      end

      raise(Session::SessionKiller, "This server requires an encrypted and authenticated connection!")
    end
  end

  def feed(data, max_length)
    # Tell the window that we're still alive
    window.kick()

    begin
      if(@encryptor)
        data = @encryptor.decrypt_packet(data)
        max_length -= 8
      end

      packet = Packet.parse(data, @options)

      if(Settings::GLOBAL.get("packet_trace"))
        window = _get_pcap_window()
        window.puts("IN:  #{packet}")
      end

      # We can send a FIN and close right away if the session was killed
      if(@state == STATE_KILLED)
        raise(Session::SessionKiller, "The session is no longer valid")
      end

      # Unless it's an encrypted packet (which implies that we're still negotiating stuff), enforce encryption restraints
      _check_crypto_options(packet)
      handler = HANDLERS[packet.type]
      if(handler.nil?)
        raise(DnscatException, "No handler found for that packet type: #{packet.type}")
      end

      # Handle the packet
      response_packet = send(handler, packet, max_length)
    rescue Session::SessionKiller => e
      @window.with({:to_ancestors => true}) do
        @window.puts("Session killed: #{e.message}")
      end
      kill()
      response_packet = Packet.create_fin(@options, {
        :session_id => @id,
        :reason => e.message,
      })
    rescue DnscatException => e
      @window.with({:to_ancestors => true}) do
        @window.puts("An error occurred (see #{@window.id} for stacktrace): #{e.message}")
      end
      @window.puts()
      @window.puts("If you think this might be a bug, please report this trace:")
      @window.puts(e.inspect)
      e.backtrace.each do |bt|
        @window.puts(bt)
      end

      response_packet = nil
    end

    # If the program needs to ignore the packet, then it returns nil, and we
    # return a bunch of nothing
    if(response_packet.nil?)
      @window.puts("OUT: <no data>")
      return ''
    end

    if(Settings::GLOBAL.get("packet_trace"))
      window = _get_pcap_window()
      window.puts("OUT: #{response_packet}")
    end

    response_bytes = response_packet.to_bytes()
    # If we're supposed to encrypt, do so
    if(@encryptor)
      response_bytes = @encryptor.encrypt_packet(response_bytes, true)
    end

    return response_bytes
  end
end
