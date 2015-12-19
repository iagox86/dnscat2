##
# dnslogger.rb
# Created July 22, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
# Parses a PCAP file, looking for dnscat2 traffic.
##

$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9
$LOAD_PATH << File.dirname(__FILE__) + "/../server"

require 'trollop'
require 'pcap'

require 'controller/packet'
require 'libs/command_packet'
require 'libs/dnser'
require 'tunnel_drivers/driver_dns'

# version info
NAME = "dnscat_parser"
VERSION = "v1.0.0"

Thread.abort_on_exception = true

# Options
opts = Trollop::options do
  version(NAME + " " + VERSION)

  opt :version, "Get the #{NAME} version",      :type => :boolean, :default => false
end

pcaps = ARGV

#if(opts[:port] < 0 || opts[:port] > 65535)
#  Trollop::die :port, "must be a valid port (between 0 and 65535)"
#end

@sessions = {}

puts("Starting #{NAME} #{VERSION}...")
pcaps.each do |pcap|
  pcap = Pcap::Capture.open_offline(pcap)

  pcap.each do |packet|
    # Use DNSer to go udp -> dns
    dns = DNSer::Packet.parse(packet.udp_data)

    # Use a tunnel_driver to go dns -> dnscat bytes
    is_incoming = false
    if(dns.qr == DNSer::Packet::QR_QUERY)
      question = dns.questions[0]
      dnscat_bytes = DriverDNS::question_to_bytes(question, ['skullseclabs.org'])
    else
      is_incoming = true
      answer = dns.answers[0]
      dnscat_bytes = DriverDNS::answer_to_bytes(answer, ['skullseclabs.org'])
    end

    # Use the Packet class to go dnscat bytes -> dnscat
    packet = Packet::parse(dnscat_bytes, 0)

    if(packet.type == Packet::MESSAGE_TYPE_SYN)
      session = @sessions[packet.session_id]
      if(session.nil?)
        session = {
          :options            => packet.body.options,
          :is_command         => (packet.body.options & Packet::OPT_COMMAND) == Packet::OPT_COMMAND,
          :my_seq             => packet.body.seq,
          :command_stream_out => "",
          :command_stream_in  => "",
          :looks_new          => false
        }
        @sessions[packet.session_id] = session

        puts(packet.to_s(false))
      else
        if(packet.body.seq == session[:my_seq])
          puts("0x%04x OUT: Duplicate SYN! Ignoring.\n" % [packet.session_id])
          next
        end
        if(packet.body.seq == session[:their_seq])
          puts("0x%04x IN:  Duplicate SYN! Ignoring.\n" % [packet.session_id])
          next
        end
        session[:their_seq] = packet.body.seq

        puts(packet.to_s(false))
      end

    elsif(packet.type == Packet::MESSAGE_TYPE_MSG)
      session = @sessions[packet.session_id]
      if(session.nil?)
        puts("WARNING: We didn't see the SYN for this session, it probably won't parse right!\n")
        session = {
          :my_seq             => 0,
          :their_seq          => 0,
          :options            => 0,
          :is_command         => false,
        }
        @sessions[packet.session_id] = session
      end

      # Parse the packet properly, now that we know the options
      packet = Packet::parse(dnscat_bytes, session[:options])

      if(session[:is_command])
        # If we see a really large request_id, it almost certainly means we're on 0.06 or higher
#        if(!session[:looks_new])
#          if((packet.get(:request_id) & 0x8000) == 0x8000)
#            puts("INFO: This session looks like new-style!")
#            session[:looks_new] = true
#          end
#        end
#
#        if(!session[:looks_new])
#          # dnscat 0.05 and below
#          is_request = !is_incoming
#        else
#          is_request = (packet.get(:request_id) & 0x8000) == 0x0000
#        end

        if(!is_incoming)
          session[:command_stream_out] += packet.body.data
          loop do
            command_packet, new_command_stream = CommandPacket.parse(session[:command_stream_out], false)
            if(command_packet.nil?)
              break
            end
            session[:command_stream_out] = new_command_stream
            puts("OUT: #{command_packet}")
          end
        else
          session[:command_stream_in] += packet.body.data
          loop do
            command_packet, new_command_stream = CommandPacket.parse(session[:command_stream_in], true)
            if(command_packet.nil?)
              break
            end

            session[:command_stream_in] = new_command_stream
            puts("IN:  #{command_packet}")
          end
        end
      else
        puts(packet.to_s(false))
      end
    elsif(packet.type == Packet::MESSAGE_TYPE_FIN)
      puts(packet.to_s(false))
    else
      puts("Sorry, we don't currently handle the type of packet we saw: 0x%02x", packet.type)
    end
  end
end
