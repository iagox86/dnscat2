# ui_session.rb
# By Ron Bowes
# Created July 4, 2013

require 'timeout'

class UiSession
  attr_accessor :id
  @@data = ""

  def initialize(id)
    @id = id
    @is_active = true
    @is_attached = false
    @orig_suspend = nil

    if(!Ui.get_option("auto_command").nil?)
      Session.find(id).queue_outgoing(Ui.get_option("auto_command"))
    end

    # TODO: Auto-attach
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
  end

  def detach()
    restore_suspend()
    @is_attached = false
  end

  def prompt_session()
    if(Ui.get_option("prompt"))
      puts("dnscat [#{id}]> ", false)
    end
  end

  def go
    # The IO.select() seems to break when a signal is caught, and it fails to
    # return (at least on cygwin, haven't tried other systems). Wrapping it in
    # a 'Timeout' block with a slightly longer intervalworks around that
    # problem.
    result = nil
    begin
      Timeout::timeout(0.5) do
        result = IO.select([$stdin], nil, nil, 0.25)
      end
    rescue Timeout::Error
    end

    # Make sure we're still in a session
    if(!active?)
      return
    end

    session = Session.find(id)

    if(!result.nil? && result.length > 0)
      line = $stdin.gets
      session.queue_outgoing(line)
    end

    # Read incoming data, if it exists
    if(session.incoming?)
      Ui.display(session.read_incoming)
    end
  end

  def data_received(data)
    # TODO: Limit the length
    @@data += data

    if(attached?)
      puts(data)
    end
  end

  def data_acknowledged(data)
    # TODO: queue
    if(attached?)
      puts()
      puts("[ACK] #{data}")
    end
  end

  def handle_suspend()
    # Trap ctrl-z, just like Metasploit
    @orig_suspend = Signal.trap("TSTP") do
      puts("TRAP")
      Ui.detach_session()
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
