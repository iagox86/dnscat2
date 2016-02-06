FROM ruby:2.1-onbuild
MAINTAINER Mark Percival <m@mdp.im>

EXPOSE 53/udp

CMD ["ruby ./dnscat2.rb"]

# Run it
#   docker run -p 53:53/udp -it --rm mpercival/dnscat2 ruby ./dnscat2.rb foo.org
