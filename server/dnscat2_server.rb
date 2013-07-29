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

require 'driver_dns'
require 'driver_tcp'
require 'test'

require 'log'
require 'packet'
require 'session'
require 'tunnel'
require 'ui'

# Option parsing
require 'trollop'

# This class should be totally stateless, and rely on the Session class
# for any long-term session storage
class Dnscat2
  @@tunnels = {}

  # Begin subscriber stuff (this should be in a mixin, but static stuff doesn't
  # really seem to work
  @@subscribers = []
  def Dnscat2.subscribe(cls)
    @@subscribers << cls
  end
  def Dnscat2.unsubscribe(cls)
    @@subscribers.delete(cls)
  end
  def Dnscat2.notify_subscribers(method, args)
    @@subscribers.each do |subscriber|
      if(subscriber.respond_to?(method))
         subscriber.method(method).call(*args)
      end
    end
  end
  # End subscriber stuff

  def Dnscat2.kill_session(session)
    if(!@@tunnels[session.id].nil?)
      @@tunnels[session.id].kill
    end

    session.destroy
  end

  def Dnscat2.handle_syn(pipe, packet, session)
    # Ignore errant SYNs - they are, at worst, retransmissions that we don't care about
    if(!session.syn_valid?())
      Dnscat2.notify_subscribers(:dnscat2_state_error, [session.id, "SYN received in invalid state"])
      return nil
    end

    session.set_their_seq(packet.seq)
    session.set_name(packet.name)
    session.set_established()

    if(!packet.tunnel_host.nil?)
      begin
        Log.WARNING("Creating a tunnel to #{packet.tunnel_host}:#{packet.tunnel_port}")
        @@tunnels[session.id] = Tunnel.new(session.id, packet.tunnel_host, packet.tunnel_port)
        @@tunnels[session.id].go
      rescue Exception => e
        Log.ERROR("Couldn't create a tunnel: #{e}")
        Dnscat2.kill_session(session)
        return Packet.create_fin(packet.packet_id, session.id)
      end
    end

    Dnscat2.notify_subscribers(:dnscat2_syn_received, [session.id, session.my_seq, packet.seq])

    return Packet.create_syn(packet.packet_id, session.id, session.my_seq, nil)
  end

  def Dnscat2.handle_msg(pipe, packet, session, max_length)
    if(!session.msg_valid?())
      Dnscat2.notify_subscribers(:dnscat2_state_error, [session.id, "MSG received in invalid state; sending FIN"])

      # Kill the session as well - in case it exists
      Dnscat2.kill_session(session)

      return Packet.create_fin(packet.packet_id, session.id)
    end

    # Validate the sequence number
    if(session.their_seq != packet.seq)
      Dnscat2.notify_subscribers(:dnscat2_msg_bad_seq, [session.their_seq, packet.seq])

      # Re-send the last packet
      old_data = session.read_outgoing(max_length - Packet.msg_header_size)
      return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, old_data)
    end

    if(!session.valid_ack?(packet.ack))
      Dnscat2.notify_subscribers(:dnscat2_msg_bad_ack, [session.my_seq, packet.ack])

      # Re-send the last packet
      old_data = session.read_outgoing(max_length - Packet.msg_header_size)
      return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, old_data)
    end

    # Acknowledge the data that has been received so far
    session.ack_outgoing(packet.ack)

    if(!@@tunnels[session.id].nil?)
      # Send the data on if it's a tunnel
      @@tunnels[session.id].send(packet.data)
    end

    # Write the incoming data to the session
    session.queue_incoming(packet.data)

    # Increment the expected sequence number
    session.increment_their_seq(packet.data.length)

    new_data = session.read_outgoing(max_length - Packet.msg_header_size)
    Dnscat2.notify_subscribers(:dnscat2_msg, [packet.data, new_data])

    # Build the new packet
    return Packet.create_msg(packet.packet_id, session.id, session.my_seq, session.their_seq, new_data)
  end

  def Dnscat2.handle_fin(pipe, packet, session)
    # Ignore errant FINs - if we respond to a FIN with a FIN, it would cause a potential infinite loop
    if(!session.fin_valid?())
      Dnscat2.notify_subscribers(:dnscat2_state_error, [session.id, "FIN received in invalid state"])
      return nil
    end

    Dnscat2.notify_subscribers(:dnscat2_fin, [session.id])

    Dnscat2.kill_session(session)
    return Packet.create_fin(packet.packet_id, session.id)
  end

  def Dnscat2.go(pipe)
    pipe.recv() do |data, max_length|
      session_id = nil

      begin
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
            raise(DnscatException, "Unknown packet type: #{packet.type}")
          end
        end

        if(response)
          Dnscat2.notify_subscribers(:dnscat2_send, [Packet.parse(response)])
          if(response.length > max_length)
            raise(RuntimeError, "Tried to send packet of #{response.length} bytes, but max_length is #{max_length} bytes")
          end
        end

        response # Return it, in a way

      rescue SystemExit
        exit

      # Catch IOErrors, but don't destroy the session - it may continue later
      rescue IOError => e
        raise(e)

      # Destroy the session on protocol errors - the client will be informed if they
      # send another message, because they'll get a FIN response
      rescue DnscatException => e
        begin
          if(!session_id.nil?)
            Log.FATAL("DnscatException caught; closing session #{session_id}...")
            Dnscat2.kill_session(session)
            Log.FATAL("Propagating the exception...")
          end
        rescue
          # Do nothing
        end

        raise(e)
      rescue Exception => e
        Log.FATAL("Fatal exception caught:")
        Log.FATAL(e.inspect)
        Log.FATAL(e.backtrace)

        exit
      end
      # Let other exceptions propagate, they will be displayed by the parent
    end
  end
end

# Subscribe the Ui to the important notifications
Session.subscribe(Ui)
Dnscat2.subscribe(Ui)
Log.subscribe(Ui)

# Options
opts = Trollop::options do
  opt :dns,       "Start a DNS server",
    :type => :boolean, :default => true
  opt :dnshost,   "The DNS ip address to listen on",
    :type => :string,  :default => "0.0.0.0"
  opt :dnsport,   "The DNS port to listen on",
    :type => :integer, :default => 53
  opt :domain,    "The DNS domain to respond to [regex, but must only match the non-dnscat portion of the string]",
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
  opt :signals,        "Use to disable signals, which break rvmsudo",
    :type => :boolean,  :default => true
end

opts[:debug].upcase!()
if(Log.get_by_name(opts[:debug]).nil?)
  Trollop::die :debug, "level values are: #{Log::LEVELS}"
  return
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
      DriverDNS.go(opts[:dnshost], opts[:dnsport], opts[:domain])
    rescue SystemExit
      exit
    rescue DnscatException => e
      Log.ERROR("Protocol exception caught in DNS module:")
      Log.ERROR(e.inspect)
    rescue Exception => e
      Log.FATAL("Fatal exception caught in DNS module:")
      Log.FATAL(e.inspect)
      Log.FATAL(e.backtrace)
      exit
    end
  end
end

if(opts[:tcp])
  threads << Thread.new do
    begin
      DriverTCP.go(opts[:tcphost], opts[:tcpport])
    rescue SystemExit
      exit
    rescue Exception => e
      Log.FATAL("Fatal exception caught in TCP module:")
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

# This is simply to give up the thread's timeslice, allowing the driver threads
# a small amount of time to initialize themselves
sleep(0.01)

Ui.set_option("auto_attach",  opts[:auto_attach])
Ui.set_option("auto_command", opts[:auto_command])
Ui.set_option("packet_trace", opts[:packet_trace])
Ui.set_option("prompt",       opts[:prompt])
Ui.set_option("log_level",    opts[:debug])
Ui.set_option("signals",      opts[:signals])

Ui.go

