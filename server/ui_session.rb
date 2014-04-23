# ui_session.rb
# By Ron Bowes
# Created July 4, 2013

require 'session_manager'
require 'timeout'

class UiSession
  attr_accessor :id

  def initialize(id)
    @id = id
    @is_active = true
    @is_attached = false
    @orig_suspend = nil
    @data = ""

    if(!Ui.get_option("auto_command").nil?)
      SessionManager.find(id).queue_outgoing(Ui.get_option("auto_command") + "\n")
    end
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
    puts(@data)
    @data = ''
  end

  def detach()
    restore_suspend()
    @is_attached = false
  end

  def go
    if(Ui.get_option("prompt"))
      line = Readline::readline("dnscat [#{id}]> ", true)
    else
      line = Readline::readline("", true)
    end

    if(line.nil?)
      return
    end

    # Add the newline that Readline strips
    line = line + "\n"

    # Find the session we're a part of
    session = SessionManager.find(id)

    # Queue our outgoing data
    session.queue_outgoing(line)

    # Read incoming data, if it exists
    if(session.incoming?)
      Ui.display(session.read_incoming)
    end
  end

  def data_received(data)
    if(attached?)
      print(data)
    else
      @data += data
    end
  end

  def data_acknowledged(data)
    if(attached?)
      puts()
      puts("[ACK] #{data}")
    else
      @data += "[ACK] #{data}"
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
