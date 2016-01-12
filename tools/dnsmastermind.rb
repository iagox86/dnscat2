##
# dnslogger.rb
# Created July 22, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
# Simply checks if you're the authoritative server.
##

$LOAD_PATH << File.dirname(__FILE__) # A hack to make this work on 1.8/1.9

require 'trollop'
require '../server/libs/dnser'

# version info
NAME = "dnsmastermind"
VERSION = "v1.0.0"

Thread.abort_on_exception = true

# Options
opts = Trollop::options do
  version(NAME + " " + VERSION)

  opt :version, "Get the #{NAME} version",                             :type => :boolean, :default => false
  opt :host,    "The ip address to listen on",                         :type => :string,  :default => "0.0.0.0"
  opt :port,    "The port to listen on",                               :type => :integer, :default => 53
  opt :timeout, "The amount of time (seconds) to wait for a response", :type => :integer, :default => 10
  opt :solution,"The answer; should be four letters, unless you're a jerk", :type => :string, :default => nil, :required => true
  opt :win,     "The message to display to winners",                   :type => :string,  :default => "YOU WIN!!"
end

if(opts[:port] < 0 || opts[:port] > 65535)
  Trollop::die :port, "must be a valid port (between 0 and 65535)"
end

if(opts[:solution].include?('.'))
  Trollop::die :solution, "must not contain period; SHOULD only contain [a-z]{4} :)"
end
solution = opts[:solution].upcase()

puts("Starting #{NAME} #{VERSION} DNS server on #{opts[:host]}:#{opts[:port]}")

dnser = DNSer.new(opts[:host], opts[:port])

dnser.on_request() do |transaction|
  begin
    request = transaction.request

    if(request.questions.length < 1)
      puts("The request didn't ask any questions!")
      next
    end

    if(request.questions.length > 1)
      puts("The request asked multiple questions! This is super unusual, if you can reproduce, please report!")
      next
    end

    if(request.questions[0].type != DNSer::Packet::TYPE_TXT)
      next
    end
    guess, domain = request.questions[0].name.split(/\./, 2)
    guess.upcase!()

    if(guess == solution)
      puts("WINNER!!!")
      answer = opts[:win]
    elsif(guess.length == solution.length)
      saved_guess = guess
      tmp_solution = solution.chars.to_a()
      guess = guess.chars.to_a()
      answer = ""

      0.upto(tmp_solution.length() - 1) do |i|
        if(tmp_solution[i] == guess[i])
          answer += "O"
          tmp_solution[i] = ""
          guess[i] = ""
        end
      end

      guess.each do |c|
        if(c == "")
          next
        end

        if(tmp_solution.include?(c))
          tmp_solution[tmp_solution.index(c)] = ""
          answer += "X"
        end
      end

      if(answer == "")
        answer = "No correct character; keep trying!"
      end

      puts("Guess: #{saved_guess} => #{answer}")
    else
      puts("Invalid; sending instructions: #{guess}")
      answer = "Instructions: guess the #{solution.length}-character string: dig -t txt [guess].#{domain}! 'O' = correct, 'X' = correct, but wrong position"
    end

    answer = DNSer::Packet::Answer.new(request.questions[0], DNSer::Packet::TYPE_TXT, DNSer::Packet::CLS_IN, 100, DNSer::Packet::TXT.new(answer))

    transaction.add_answer(answer)
    transaction.reply!()
  rescue StandardError => e
    puts("Error: #{e}")
    puts(e.backtrace)
  end
end

dnser.wait()
