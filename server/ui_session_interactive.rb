# ui_session_interactive.rb
# By Ron Bowes
# Created July 4, 2013

require 'ui_interface_with_id'

class UiSessionInteractive < UiInterfaceWithId
  attr_accessor :id
  attr_accessor :session

  MAX_HISTORY_LENGTH = 10000

  def initialize(id, session, ui)
    super(id)

    @id = id
    @session  = session
    @ui = ui

    if(!@ui.get_option("auto_command").nil? && @ui.get_option("auto_command").length > 0)
      @session.queue_outgoing(@ui.get_option("auto_command") + "\n")
    end
  end

  def to_s()
    if(active?())
      idle = Time.now() - @last_seen
      if(idle > 120)
        return "%ssession %d :: %s :: [idle for over two minutes; probably dead]" % [activity_indicator(), @id, @session.name]
      elsif(idle > 5)
        return "%ssession %d :: %s :: [idle for %d seconds]" % [activity_indicator(), @id, @session.name, idle]
      else
        return "%ssession %d :: %s" % [activity_indicator(), @id, @session.name]
      end
    else
      return "%ssession %d :: %s :: [closed]" % [activity_indicator(), @id, @session.name]
    end
  end

  def attach()
    super

    if(!active?())
      Log.WARNING(@id, "This session is closed!")
      return false
    end

    return true
  end

  def go
    line = Readline::readline("", true)

    if(line.nil?)
      return
    end

    # Add the newline that Readline strips
    line = line + "\n"

    # Queue our outgoing data
    @session.queue_outgoing(line)
  end

  def feed(data)
    seen()

    print(data)

  end

  def output(str)
    # I don't think this is necessary
    #raise(DnscatException, "I don't think I use this")
    puts(str)
  end

  def error(str)
    puts("#{str}")
  end

  def ack(data)
    seen()
    #display(data, '[OUT]')
  end
end
