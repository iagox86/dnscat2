FROM ruby:2.5.1
MAINTAINER Mofisto
MAINTAINER Mofisto
EXPOSE 53/udp
RUN apt-get update -qq  && apt-get install gem  libpq-dev -y && mkdir /myapp
WORKDIR /myapp
COPY Gemfile /myapp/Gemfile
RUN bundle install

# Run it
#   docker run -p 53:53/udp -it -v "$PWD":/usr/src/myapp -w /usr/src/myapp --rm dnscat2 ruby ./dnscat2.rb Examples.com
# Tips
#You should currently be on dnscat2 / server
#   docker build -t dnscat2 .
