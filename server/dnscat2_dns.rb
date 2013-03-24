##
# dnscat2_dns.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# The DNS dnscat server.
##
$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'rubygems'
require 'rubydns'

require 'dnscat2_server'
require 'log'

#      # Create a DNS server if we don't have one already
#  IN = Resolv::DNS::Resource::IN
#      RubyDNS::run_server(:listen => [[:udp, "0.0.0.0", 53]]) do
#        match(/.*/, IN::A) do |transaction|
#          transaction.respond!("1.2.3.4")
#        end
#        match(/.*/, IN::MX) do |transaction|
#          transaction.respond!(1, ["www.google.ca"])
#        end
#        match(/.*/, IN::TXT) do |transaction|
#          transaction.respond!("hello")
#        end
#      end

class DnscatDNS

  def max_packet_size()
    return 32 # TODO: do this better
  end

  def initialize()
    Thread.new() do
    end
  end

  def send(data)
    # TODO
    data = [data.length, data].pack("nA*")
    @s.write(data)
  end

  def recv()
    length = @s.read(2)
    if(length.nil? || length.length != 2)
      raise(IOError, "Connection closed while reading the length")
    end
    length = length.unpack("n").shift

    data = @s.read(length)
    if(data.nil? || data.length != length)
      raise(IOError, "Connection closed while reading packet")
    end

    return data
  end

  def close()
    @s.close
  end
end

