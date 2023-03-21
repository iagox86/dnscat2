FROM ruby:3.2.1
MAINTAINER Mark Percival <m@mdp.im>

# Install dependencies
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update -qq \
    && apt-get install gem libpq-dev -y \
    && mkdir -p /app \
    && rm -rf /var/lib/apt/lists/*

COPY . /app
WORKDIR /app
RUN bundle install

EXPOSE 53/udp

ENTRYPOINT ["ruby", "dnscat2.rb"]
