# ui_session_interactive.rb
# By Ron Bowes
# Created July 4, 2013

require 'ui_interface'

class UiSessionInteractive < UiInterface
  attr_accessor :local_id
  attr_accessor :session

  MAX_HISTORY_LENGTH = 10000

  def initialize(local_id, session, ui)
    super()

    @local_id = local_id
    @session  = session
    @ui = ui

    @history = []
    @last_seen = Time.now()

    if(!@ui.get_option("auto_command").nil? && @ui.get_option("auto_command").length > 0)
      @session.queue_outgoing(@ui.get_option("auto_command") + "\n")
    end
  end

  def get_history()
    return @history.join("\n")
  end

  def display(str, tag)
    # Split the lines up
    lines = str.chomp().gsub(/\r/, '').split(/\n/)

    # Display them and add them to history
    lines.each do |line|
      if(attached?())
        puts("%s %s" % [tag, line])
      end
      @history << ("%s %s" % [tag, line])
    end

    # Shorten history if needed
    while(@history.length > MAX_HISTORY_LENGTH) do
      @history.shift()
    end
  end

  def to_s()
    if(@state.nil?)
      idle = Time.now() - @last_seen
      if(idle > 60)
        return "session %5d :: %s :: [idle for over a minute; probably dead]" % [@local_id, @session.name]
      elsif(idle > 5)
        return "session %5d :: %s :: [idle for %d seconds]" % [@local_id, @session.name, idle]
      else
        return "session %5d :: %s" % [@local_id, @session.name]
      end
    else
      return "session %5d :: %s :: [%s]" % [@local_id, @session.name, @state]
    end
  end

  def attach()
    super

    # Print the queued data
    puts(get_history())

#    if(!@state.nil?)
#      Log.WARNING("This session is #{@state}! Closing...")
#      return false
#    end

    return true
  end

  def go
    if(@ui.get_option("prompt"))
      line = Readline::readline("dnscat [#{@local_id}]> ", true)
    else
      line = Readline::readline("", true)
    end

    if(line.nil?)
      return
    end

    # Add the newline that Readline strips
    line = line + "\n"

    # Queue our outgoing data
    @session.queue_outgoing(line)
  end

  def feed(data)
    @last_seen = Time.now()
    display(data, '[IN] ')
  end

  def output(str)
    display(str)
  end

  def error(str)
    display(str, "[ERROR]")
  end

  def ack(data)
    @last_seen = Time.now()
    display(data, '[OUT]')
  end

  def heartbeat()
    @last_seen = Time.now()
  end
end
