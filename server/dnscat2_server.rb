##
# dnscat2_server.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# Implements basically the full Dnscat2 protocol. Doesn't care about
# lower-level protocols.
##

$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9
$LOAD_PATH << File.dirname(__FILE__) + '/rubydns/lib' # TODO: Is this still required

require 'dnscat2_dns'
require 'dnscat2_tcp'
require 'dnscat2_test'

require 'log'
require 'packet'
require 'session'
require 'ui'

# Option parsing
require 'trollop'

# This class should be totally stateless, and rely on the Session class
# for any long-term session storage
class Dnscat2
  # Begin subscriber stuff (this should be in a mixin, but static stuff doesn't
  # really seem to work
  @@subscribers = []
  @@mutex = Mutex.new()
  def Dnscat2.subscribe(cls)
#    @@mutex.lock() do
      @@subscribers << cls
#    end
  end
  def Dnscat2.unsubscribe(cls)
#    @@mutex.lock() do
      @@subscribers.delete(cls)
#    end
  end
  def Dnscat2.notify_subscribers(method, args)
#    @@mutex.lock do
      @@subscribers.each do |subscriber|
        if(subscriber.respond_to?(method))
           subscriber.method(method).call(*args)
        end
      end
#    end
  end
  # End subscriber stuff

  def Dnscat2.handle_syn(pipe, packet, session)
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!session.syn_valid?())
      Log.WARNING("SYN invalid in this state (ignored)")
      return nil
    end

    session.set_their_seq(packet.seq)
    session.set_name(packet.name)
    session.set_established()

    Dnscat2.notify_subscribers(:dnscat2_syn_received, [session.my_seq, packet.seq])

    return Packet.create_syn(packet.packet_id, session.id, session.my_seq, nil)
  end

  def Dnscat2.handle_msg(pipe, packet, session, max_length)
    if(!session.msg_valid?())
      Log.WARNING("MSG invalid in this state (responding with an error)")
      return Packet.create_fin(packet.packet_id, session.id)
    end

    # Validate the sequence number
    if(session.their_seq != packet.seq)
      Log.WARNING("Bad sequence number; expected 0x%04x, got 0x%04x [re-sending]" % [session.their_seq, packet.seq])
      Dnscat2.notify_subscribers(:dnscat2_msg_bad_seq, [session.their_seq, packet.seq])

      # Re-send the last packet
      old_data = session.read_outgoing(max_length - Packet.msg_header_size)
      return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, old_data)
    end

    if(!session.valid_ack?(packet.ack))
      Log.WARNING("Impossible ACK received: 0x%04x, current SEQ is 0x%04x [re-sending]" % [packet.ack, session.my_seq])
      Dnscat2.notify_subscribers(:dnscat2_msg_bad_ack, [session.my_seq, packet.ack])

      # Re-send the last packet
      old_data = session.read_outgoing(max_length - Packet.msg_header_size)
      return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, old_data)
    end

    # Acknowledge the data that has been received so far
    session.ack_outgoing(packet.ack)

    # Write the incoming data to the session
    session.queue_incoming(packet.data)

    # Increment the expected sequence number
    session.increment_their_seq(packet.data.length)

    new_data = session.read_outgoing(max_length - Packet.msg_header_size)
    Log.INFO("Received MSG with #{packet.data.length} bytes; responding with our own message (#{new_data.length} bytes)")
    Dnscat2.notify_subscribers(:dnscat2_msg, [packet.data, new_data])

    # Build the new packet
    return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, new_data)
  end

  def Dnscat2.handle_fin(pipe, packet, session)
    # Ignore errant FINs - if we respond to a FIN with a FIN, it would cause a potential infinite loop
    if(!session.fin_valid?())
      Log.WARNING("FIN invalid in this state")
      return nil
    end

    Log.WARNING("Received FIN for session #{session.id}; closing session")
    Dnscat2.notify_subscribers(:dnscat2_fin, [])

    session.destroy()
    return Packet.create_fin(packet.packet_id, session.id)
  end

  def Dnscat2.go(pipe)
    begin
      session_id = nil

      pipe.recv() do |data, max_length|
        packet = Packet.parse(data)

        Dnscat2.notify_subscribers(:dnscat2_recv, [packet])

        # Store the session_id in a variable so we can close it if there's a problem
        session_id = packet.session_id

        # Find the session
        session = Session.find(packet.session_id)

        response = nil
        if(session.nil?)
          if(packet.type == Packet::MESSAGE_TYPE_SYN)
            # If the session doesn't exist, and it's a SYN, create it
            session = Session.create_session(packet.session_id)
          else
            # If the session doesn't exist
            response = Packet.create_fin(packet.packet_id, packet.session_id)
          end
        end

        if(!session.nil?)
          if(packet.type == Packet::MESSAGE_TYPE_SYN)
            response = handle_syn(pipe, packet, session)
          elsif(packet.type == Packet::MESSAGE_TYPE_MSG)
            response = handle_msg(pipe, packet, session, max_length)
          elsif(packet.type == Packet::MESSAGE_TYPE_FIN)
            response = handle_fin(pipe, packet, session)
          else
            raise(IOError, "Unknown packet type: #{packet.type}")
          end
        end

        if(response)
          Dnscat2.notify_subscribers(:dnscat2_send, [Packet.parse(response)])
          if(response.length > max_length)
            raise(IOError, "Tried to send packet of #{response.length} bytes, but max_length is #{max_length} bytes")
          end
        end

        response # Return it, in a way
      end
    rescue IOError => e
      # Don't destroy the session on IOErrors, a new connection can resume the same session
      raise(e)

    rescue Exception => e
      # Destroy the session on non-IOError exceptions
      begin
        if(!session_id.nil?)
          Log.FATAL("Exception thrown; attempting to close session #{session_id}...")
          Session.destroy(session_id)
          Log.FATAL("Propagating the exception...")
        end
      rescue
        # Do nothing
      end

      raise(e)
    end
  end
end


# Options
opts = Trollop::options do
  opt :dns,       "Start a DNS server",
    :type => :boolean, :default => true
  opt :dnshost,   "The DNS ip address to listen on",
    :type => :string,  :default => "0.0.0.0"
  opt :dnsport,   "The DNS port to listen on",
    :type => :integer, :default => 53
  opt :domain,    "The DNS domain to respond to [regex]",
    :type => :string,  :default => "skullseclabs.org"

  opt :tcp,       "Start a TCP server",
    :type => :boolean, :default => true
  opt :tcphost,   "The TCP ip address to listen on",
    :type => :string,  :default => "0.0.0.0"
  opt :tcpport,    "The port to listen on",
    :type => :integer, :default => 4444

  opt :debug,     "Min debug level [info, warning, error, fatal]",
    :type => :string,  :default => "warning"

  opt :do_tests,      "If set, test the session code instead of actually going",
    :type => :boolean, :default => false

  opt :auto_attach, "If set to 'false', don't auto-attach to clients when no client is specified",
    :type => :boolean, :default => true
  opt :auto_command,   "Send this to each client that connects",
    :type => :string,  :default => nil
  opt :packet_trace,   "Display incoming/outgoing dnscat packets",
    :type => :boolean,  :default => false
  opt :prompt,         "Display a prompt during sessions",
    :type => :boolean,  :default => false
end

puts("debug = #{opts[:debug]}")
case opts[:debug]
when "info"
  Log.set_min_level(Log::INFO)
when "warning"
  Log.set_min_level(Log::WARNING)
when "error"
  Log.set_min_level(Log::ERROR)
when "fatal"
  Log.set_min_level(Log::FATAL)
else
  Trollop::die :debug, "must be 'info', 'warning', 'error', or 'fatal'"
end

if(opts[:dnsport] < 0 || opts[:dnsport] > 65535)
  Trollop::die :dnsport, "must be a valid port"
end

if(opts[:tcpport] < 0 || opts[:tcpport] > 65535)
  Trollop::die :dnsport, "must be a valid port"
end

threads = []
if(opts[:dns])
  threads << Thread.new do
    begin
      DnscatDNS.go(opts[:dnshost], opts[:dnsport], opts[:domain])
    rescue SystemExit
      exit
    rescue Exception => e
      Log.FATAL("Exception caught in DNS module:")
      Log.FATAL(e.inspect)
      Log.FATAL(e.backtrace)
      exit
    end
  end
end

if(opts[:tcp])
  threads << Thread.new do
    begin
      DnscatTCP.go(opts[:tcphost], opts[:tcpport])
    rescue SystemExit
      exit
    rescue Exception => e
      Log.FATAL("Exception caught in TCP module:")
      Log.FATAL(e.inspect)
      Log.FATAL(e.backtrace)
      exit
    end
  end
end


if(threads.length == 0)
  Log.FATAL("No UI was started! Use --dns or --tcp!")
  exit
end

if(opts[:do_tests])
  DnscatTest.do_test()
end

Ui.set_option("auto_attach",  opts[:auto_attach])
Ui.set_option("auto_command", opts[:auto_command])
Ui.set_option("packet_trace", opts[:packet_trace])
Ui.set_option("prompt",       opts[:prompt])
Ui.go

