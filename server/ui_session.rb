# ui_session.rb
# By Ron Bowes
# Created July 4, 2013

require 'timeout'

class UiSession
  attr_accessor :local_id
  attr_accessor :session

  HISTORY_MAX_LENGTH = 10000


  def initialize(local_id, session)
    @local_id = local_id
    @session  = session
    @history = []

    @is_active = true
    @is_attached = false
    @orig_suspend = nil # Used for trapping ctrl-z

    if(!Ui.get_option("auto_command").nil? && Ui.get_option("auto_command").length > 0)
      @session.queue_outgoing(Ui.get_option("auto_command") + "\n")
    end
  end

  # Implements a very, very simple ring buffer
  def add_history(str)
    str = str.chomp().gsub(/\r/, '')
    @history += str.split(/\n/)
    if(@history.length > HISTORY_MAX_LENGTH)
      @history.shift()
    end
  end

  def get_history()
    return @history.join("\n")
  end

  def destroy()
    @is_active = false
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
  end

  def detach()
    restore_suspend()
    @is_attached = false
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

    # Add it to our history
    add_history("[OUT] %s" % line)

    # Read incoming data, if it exists
    # TODO: Does this actually work?
    if(@session.incoming?)
      Ui.display(@session.read_incoming)
    end
  end

  def data_received(data)
    if(attached?)
      print(data)
    end

    add_history("[IN]  %s" % data)
  end

  def data_acknowledged(data)
    if(attached?)
      puts()
    end

    data = data.chomp().gsub(/\r/, '')
    data = data.split(/\n/)

    data.each do |d|
      if(attached?)
        puts("[ACK] #{d}")
      end
      add_history("[ACK] %s" % d)
    end
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
