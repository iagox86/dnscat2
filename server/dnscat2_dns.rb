require 'rubygems'
require 'rubydns'

IN = Resolv::DNS::Resource::IN
RubyDNS::run_server(:listen => [[:udp, "0.0.0.0", 53]]) do
  match(/.*/, IN::A) do |transaction|
    transaction.respond!("1.2.3.4")
  end

  match(/.*/, IN::MX) do |transaction|
    transaction.respond!(1, ["www.google.ca"])
  end

  match(/.*/, IN::TXT) do |transaction|
    transaction.respond!("hello")
  end
end

