##
# session.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'securerandom'
require 'ecdsa'

require 'controller/packet'
require 'drivers/driver_command'
require 'drivers/driver_console'
require 'drivers/driver_process'
require 'libs/commander'
require 'libs/dnscat_exception'
require 'libs/sha3'
require 'libs/swindow'

class Session
  @@isn = nil # nil = random

  attr_reader :id, :name, :options, :state
  attr_reader :window

  # The session was just created and hasn't seen a packet yet
  STATE_NEW           = 0x00

  # After performing the key exchange
  STATE_READY         = 0x01

  # After performing authentication
  STATE_AUTHENTICATED = 0x02

  # After receiving a SYN
  STATE_ESTABLISHED   = 0x03

  # After being manually killed
  STATE_KILLED        = 0xFF

  HANDLERS = {
    Packet::MESSAGE_TYPE_SYN    => :_handle_syn,
    Packet::MESSAGE_TYPE_MSG    => :_handle_msg,
    Packet::MESSAGE_TYPE_FIN    => :_handle_fin,
    Packet::MESSAGE_TYPE_NEGENC => :_handle_negenc,
    Packet::MESSAGE_TYPE_AUTH   => :_handle_auth,
  }

  ECDH_GROUP = ECDSA::Group::Nistp256

  def initialize(id, main_window)
    @state = STATE_NEW
    @their_seq = 0
    @my_seq    = @@isn.nil? ? rand(0xFFFF) : @@isn
    @options = 0

    @secret = nil # SecureRandom.random_bytes(128)

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
    # This is basically where all the access control security happens
    if(!Settings::GLOBAL.get("require_enc"))
      return @state == STATE_NEW || @state == STATE_READY || @state == STATE_AUTHENTICATED
    elsif(!Settings::GLOBAL.get("require_auth"))
      return @state == STATE_READY || @state == STATE_AUTHENTICATED
    end

    return @state == STATE_AUTHENTICATED
  end

  def _msg_valid?()
    return @state == STATE_ESTABLISHED || @state == STATE_KILLED
  end

  def _fin_valid?()
    return @state == STATE_ESTABLISHED || @state == STATE_KILLED
  end

  def _negenc_valid?()
    return true
  end

  def _auth_valid?()
    return @state == STATE_READY
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
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    # TODO: This has an interesting impact on encrypted connections - they can get a SYN response before negotiating encryption
    if(!_syn_valid?())
      if(@their_seq == packet.body.seq && @options == packet.body.options)
        @window.puts("[WARNING] Duplicate SYN received!")
        return Packet.create_syn(0, :session_id => @id, :seq => @my_Seq)
      else
        return nil
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

    # Feed the auto_command into the window, as if it was user input
    if(auto_command = Settings::GLOBAL.get("auto_command"))
      auto_command.split(";").each do |command|
        command = command.strip()
        window.fake_input(command)
      end
    end

    return Packet.create_syn(0, :session_id => @id, :seq => @my_seq)
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

  def _handle_negenc(packet, max_length)
    if(!_negenc_valid?())
      raise(DnscatException, "NEGENC received in invalid state")
    end

    # Generate a keypair
    # TODO: Support re-negotiation
    if(@my_private_key.nil?)
      @my_private_key = 1 + SecureRandom.random_number(ECDH_GROUP.order - 1)
      @my_public_key  = ECDH_GROUP.generator.multiply_by_scalar(@my_private_key)
    end
    @window.puts(packet)
    @window.puts("X: #{packet.body.public_key_x}")
    @window.puts("Y: #{packet.body.public_key_y}")
    @their_public_key = ECDSA::Point.new(ECDH_GROUP, packet.body.public_key_x, packet.body.public_key_y)

    @shared_secret = @their_public_key.multiply_by_scalar(@my_private_key)

    @their_write_key  = Digest::SHA3.new(256).digest([@shared_secret.x.to_s(16)].pack("H*") + "client_write_key")
    @their_mac_key    = Digest::SHA3.new(256).digest([@shared_secret.x.to_s(16)].pack("H*") + "client_mac_key")
    @server_write_key = Digest::SHA3.new(256).digest([@shared_secret.x.to_s(16)].pack("H*") + "server_write_key")
    @server_mac_key   = Digest::SHA3.new(256).digest([@shared_secret.x.to_s(16)].pack("H*") + "server_mac_key")

    @window.puts("My private key:   #{@my_private_key.to_s(16)}")
    @window.puts("My public key:    #{@my_public_key.x.to_s(16)} #{@my_public_key.y.to_s(16)}")
    @window.puts("Their public key: #{@their_public_key.x.to_s(16)} #{@their_public_key.y.to_s(16)}")
    @window.puts("Shared secret: #{@shared_secret.x.to_s(16)} #{@shared_secret.y.to_s(16)}")


    return Packet.create_negenc(@options, {
      :session_id   => @id,
      :flags        => 0,
      :public_key_x => @my_public_key.x,
      :public_key_y => @my_public_key.y,
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

    # TODO: Don't allow encryption negotiation to be skipped (if the user chooses)
    packet = Packet::parse(data, @options)

    if(@shared_secret)
      encrypted_packet = Packet::EncryptedPacket.parse(data, @their_mac_key, @their_write_key, @options)
      packet = encrypted_packet.packet
      puts(encrypted_packet)
    else
      packet = Packet.parse(data, @options)
    end

    puts(packet)

    if(Settings::GLOBAL.get("packet_trace"))
      window = _get_pcap_window()
      window.puts("IN:  #{packet}")
    end

    begin
      handler = HANDLERS[packet.type]
      if(handler.nil?)
        raise(DnscatException, "No handler found for that packet type: #{packet.type}")
      end

      response_packet = send(handler, packet, max_length)
    rescue DnscatException => e
      @window.with({:to_ancestors => true}) do
        @window.puts("ERROR: Protocol exception occurred: #{e}")
        @window.puts("Switch to window #{@window.id} for more details!")
      end
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
