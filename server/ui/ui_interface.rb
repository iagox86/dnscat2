##
# ui_interface.rb
# By Ron Bowes
# Created May, 2014
#
# See LICENSE.md
##

class UiInterface
  attr_accessor :parent

  def initialize()
    @readline_history = []

    @is_active = true
    @is_attached = false
    @activity = false

    @last_seen = Time.now()

    @history = ""

    @pending_sessions = {}

    @parent = nil
  end

  def save_history()
    @readline_history = []
    Readline::HISTORY.each do |i|
      @readline_history << i
    end
  end

  def restore_history()
    @readline_history = @readline_history || []

    Readline::HISTORY.clear()
    @readline_history.each do |i|
      Readline::HISTORY << i
    end
  end

  def print(data)
    if(@is_attached)
      $stdout.print(data)
    else
      @activity = true
    end
    @history += data
  end

  def puts(data = nil)
    if(@is_attached)
      $stdout.puts(data)
    else
      @activity = true
    end

    if(!data.nil?)
      @history += data.to_s
    end

    @history += "\n"
  end

  def feed(data)
    raise("Not implemented")
  end

  def ack(data)
    # Do nothing by default
  end

  def destroy()
    @is_active = false
  end

  def heartbeat()
    seen()
  end

  def output(str)
    raise("Not implemented")
  end

  def error(str)
    raise("Not implemented")
  end

  def attach()
    #$stdout.puts("\n" * 1000)
    $stdout.puts()
    restore_history()
    catch_suspend()

    $stdout.print(@history)

    @is_attached = true
    @activity = false

    if(!active?())
      $stderr.puts()
      $stderr.puts("Session is no longer active...")
      $stderr.puts("Press <enter> to go back")
      $stdin.gets()
      @ui.detach_session()
    end
  end

  def catch_suspend()
    # Trap ctrl-z, just like Metasploit
    @orig_suspend = Signal.trap("TSTP") do
      @ui.detach_session()
    end
  end

  def detach()
    save_history()
    fix_suspend()

    @is_attached = false
  end

  def fix_suspend()
    if(@orig_suspend.nil?)
      Signal.trap("TSTP", "DEFAULT")
    else
      Signal.trap("TSTP", @orig_suspend)
    end
  end

  def active?()
    return @is_active
  end

  def attached?()
    return @is_attached
  end

  def activity?()
    return @activity
  end

  def activity_indicator()
    if(@activity)
      return "[*] "
    else
      return ""
    end
  end

  def go()
    raise("Not implemented")
  end

  def seen()
    @last_seen = Time.now()
  end
end
