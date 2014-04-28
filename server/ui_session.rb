# ui_session.rb
# By Ron Bowes
# Created July 4, 2013

require 'timeout'

class UiSession
  attr_accessor :local_id
  attr_accessor :session

  MAX_HISTORY_LENGTH = 10000

  def initialize(local_id, session)
    @local_id = local_id
    @session  = session
    @history = []
    @state = nil

    @is_active = true
    @is_attached = false
    @orig_suspend = nil # Used for trapping ctrl-z

    if(!Ui.get_option("auto_command").nil? && Ui.get_option("auto_command").length > 0)
      @session.queue_outgoing(Ui.get_option("auto_command") + "\n")
    end
  end

  def get_history()
    return @history.join("\n")
  end

  def destroy()
    @is_active = false
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

  def active?()
    return @is_active
  end

  def attached?()
    return @is_attached
  end

  def attach()
    @is_attached = true
    handle_suspend()

    # Print the queued data
    puts(get_history())

    if(!@state.nil?)
      Log.WARNING("This session is #{@state}! Closing...")
      return false
    end

    return true
  end

  def detach()
    restore_suspend()
    @is_attached = false
  end

  def set_state(state)
    @state = state
  end

  def get_summary()
    return "session %5d :: %s :: [%s]" % [@local_id, @session.name, @state.nil? ? "active" : @state]
  end

  def go
    if(Ui.get_option("prompt"))
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

  def data_received(data)
    display(data, '[IN] ')
  end

  def data_acknowledged(data)
    display(data, '[OUT]')
  end

  def handle_suspend()
    # Trap ctrl-z, just like Metasploit
    @orig_suspend = Signal.trap("TSTP") do
      Ui.detach_session()
      Ui.wakeup()
    end
  end

  def restore_suspend()
    if(@orig_suspend.nil?)
      Signal.trap("TSTP", "DEFAULT")
    else
      Signal.trap("TSTP", @orig_suspend)
    end
  end
end
