# ui_session.rb
# By Ron Bowes
# Created July 4, 2013

require 'timeout'
require 'command_packet'
require 'command_packet_stream'

class UiSessionCommand
  attr_accessor :local_id
  attr_accessor :session

  MAX_HISTORY_LENGTH = 10000

  def initialize(local_id, session)
    @local_id = local_id
    @session  = session
    @history = []
    @state = nil
    @last_seen = Time.now()

    @stream = CommandPacketStream.new()

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

  def display(str, tag = nil)
    # Split the lines up
    lines = str.chomp().gsub(/\r/, '').split(/\n/)

    # Display them and add them to history
    lines.each do |line|
      if(attached?())
        if(tag.nil?)
          puts("%s" % [line])
        else
          puts("%s %s" % [tag, line])
        end
      end
      if(tag.nil?)
        @history << ("%s" % [tag, line])
      else
        @history << ("%s %s" % [tag, line])
      end
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

  def to_s()
    if(@state.nil?)
      idle = Time.now() - @last_seen
      if(idle > 60)
        return "session %5d :: %s :: [idle for over a minute; probably dead]" % [@local_id, @session.name]
      elsif(idle > 5)
        return "session %5d :: %s :: [idle for %d seconds]" % [@local_id, @session.name, idle]
      else
        return "session %5d :: %s" % [@local_id, @session.name, idle]
      end
    else
      return "session %5d :: %s :: [%s]" % [@local_id, @session.name, @state.nil? ? "active" : @state]
    end
  end

  def do_send_ping()
    request_id = rand(0xFFFF)
    data = "this is ping data"

    ping = CommandPacket.create_ping_request(request_id, data)

    @session.queue_outgoing(ping)
  end

  def do_send_shell()
    request_id = rand(0xFFFF)
    name = "shell name"

    shell = CommandPacket.create_shell_request(request_id, name)
    @session.queue_outgoing(shell)
  end

  def do_send_exec()
    request_id = rand(0xFFFF)
    name = "exec name"
    command = "exec command"

    exec = CommandPacket.create_exec_request(request_id, name, command)
    @session.queue_outgoing(exec)
  end

  def go
    begin
      display("1) send ping")
      display("2) send shell")
      display("3) send exec")
      display('')
      display("9) quit")

      loop do
        line = Readline::readline("Your command? [#{@local_id}]> ", true)

        if(line.nil?)
          return
        end

        line = line.to_i()

        if(line == 1)
          do_send_ping()
          break
        elsif(line == 2)
          do_send_shell()
          break
        elsif(line == 3)
          do_send_exec()
          break
        elsif(line == 9)
          exit(1)
        end

        display("Unknown selection!")
      end

    rescue Exception => e
      Log.ERROR(e.inspect)
      Log.ERROR(e.backtrace)

      raise(e)
    end

  end

  def data_received(data)
    @last_seen = Time.now()

    @stream.feed(data, false) do |packet|
      display(packet.to_s, '[IN] ')
    end
  end

  def data_acknowledged(data)
    @last_seen = Time.now()
  end

  def heartbeat()
    @last_seen = Time.now()
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
