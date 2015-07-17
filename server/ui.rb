##
# ui.rb
# Created June 20, 2013
# By Ron Bowes
#
# See LICENSE.md
##

require 'trollop' # We use this to parse commands
require 'readline' # For i/o operations

require 'log'
require 'subscribable'
require 'ui_command'
require 'ui_session_command'
require 'ui_session_interactive'

class Ui
  include Subscribable

  attr_reader :settings

  def initialize(settings)
    @settings = settings
    @thread = Thread.current()

    # There's always a single UiCommand in existance
    @command = nil

    # This is a handle to the current UI the user is interacting with
    @ui = nil

    # These are the available UIs, by session id
    @uis = {}

    # Lets us have multiple 'attached' sessions
    @ui_history = []

    initialize_subscribables()
  end

  class UiWakeup < Exception
    # Nothing required
  end

  def get_by_id(id)
    return @uis[id]
  end

  # TODO: This should probably be implemented in class Settings
  def set_option(name, value)
    # Remove whitespace
    name  = name.to_s
    value = value.to_s

    name   = name.gsub(/^ */, '').gsub(/ *$/, '')
    value = value.gsub(/^ */, '').gsub(/ *$/, '')

    if(value == "" || value == "nil")
      @settings.set(name, nil)

      puts("#{name} => [deleted]")
    else
      # Replace \n with actual newlines
      value = value.gsub(/\\n/, "\n")

      # Replace true/false with the proper values
      value = true if(value == "true")
      value = false if(value == "false")

      @settings.set(name, value)
    end
  end

  def error(msg, id = nil)
    # Try to use the provided id first
    if(!id.nil?)
      ui = @uis[id]
      if(!ui.nil?)
        ui.error(msg)
        return
      end
    end

    # As a fall-back, or if the id wasn't provided, output to the current or
    # the command window
    if(@ui.nil?)
      @command.error(msg)
    else
      @ui.error(msg)
    end
  end

  # Detach the current session and attach a new one
  def attach_session(ui = nil)
    if(ui.nil?)
      ui = @command
    end

    # If the ui isn't changing, don't
    if(ui == @ui)
      return
    end

    # Detach the old ui
    if(!@ui.nil?)
      @ui_history << @ui
      @ui.detach()
    end

    # Go to the new ui
    @ui = ui

    # Attach the new ui
    @ui.attach()

    wakeup()
  end

  def detach_session()
    ui = @ui_history.pop()

    if(ui.nil?)
      ui = @command
    end

    if(!@ui.nil?)
      @ui.detach()
    end

    @ui = ui
    @ui.attach()

    wakeup()
  end

  def go()
    # Ensure that USR1 does nothing, see the 'hacks' section in the file
    # comment
#    Signal.trap("USR1") do
#      # Do nothing
#    end

    # There's always a single UiCommand in existance
    if(@command.nil?)
      @command = UiCommand.new(self)
    end

    begin
      attach_session(@command)
    rescue UiWakeup
      # Ignore
    end

    loop do
      begin
        if(@ui.nil?)
          Log.ERROR(nil, "@ui ended up nil somehow!")
        end

        # If the ui is no longer active, switch to the @command window
        if(!@ui.active?)
          @ui.error("UI went away...")
          detach_session()
        end

        @ui.go()

      rescue UiWakeup
        # Ignore the exception, it's just to break us out of the @ui.go() function
      rescue Exception => e
        puts(e)
        raise(e)
      end
    end
  end

  def each_ui()
    @uis.each_value do |s|
      yield(s)
    end
  end

  # TODO: When I fix session nesting, get rid of this
  def get_command()
    return @command
  end

  #################
  # The rest of this are callbacks
  #################

  def session_established(id)
    # Get a handle to the session
    session = SessionManager.find(id)

    # Fail if it doesn't exist
    if(session.nil?)
      raise(DnscatException, "Couldn't find the new session!")
    end

    # Create a new UI
    if(session.is_command)
      ui = UiSessionCommand.new(id, session, self)
    else
      ui = UiSessionInteractive.new(id, session, self)
    end
    self.subscribe(ui)

    # Save it in both important lists
    @uis[id] = ui

    # Let all the other sessions know that this one was created
    notify_subscribers(:ui_created, [ui, id])

    # If nobody else has claimed it, bequeath it to the root (command) ui
    # TODO: I don't really have a good way of nesting sessions right now, so just don't
    #if(ui.parent.nil?)
      ui.parent = @command

      # Since the @command window has no way to know that it's supposed to have
      # this session, add it manually
      @command.ui_created(ui, id, true)
    #else
      #ui.parent.output("New session established: #{id}")
    #end

    # Print to the main window and the current window
    $stdout.puts("New session established: #{id}")
#    if(ui.parent == @command)
#      Log.PRINT(nil, "New session established: #{id}")
#    else
#      Log.PRINT(nil, "New session established: #{id}")
#      Log.PRINT(ui.parent.id, "New session established: #{id}")
#    end

    # Auto-attach if necessary
    if(@settings.get("auto_attach"))
      attach_session(ui)

      Log.INFO(id, "(auto-attached)")
    end

    @settings.set("newest", id.to_s)
  end

  def session_data_received(id, data)
    ui = @uis[id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{id}")
    end
    ui.feed(data)
  end

  def session_data_acknowledged(id, data)
    ui = @uis[id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{id}")
    end
    ui.ack(data)
  end

  def session_destroyed(id)
    ui = @uis[id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{id}")
    end

    # Tell the UI it's been destroyed
    ui.destroy()

    # Switch the session for @command if it's attached
    if(@ui == ui)
      detach_session()
    end
  end

  # This is used by the 'kill' command the user can enter
  def kill_session(id)
    # Find the session
    ui = @uis[id]
    if(ui.nil?())
      return false
    end

    # Kill it
    ui.session.kill()

    return true
  end

  # Callback
  def session_heartbeat(id)
    ui = @uis[id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{id}")
    end
    ui.heartbeat()
  end

  # Callback
  def dnscat2_session_error(id, message)
    ui = @uis[id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{id}")
    end
    ui.error(message)
  end

#  # Callback
#  def log(level, message)
#    # Handle the special case, before a level is set
#    if(@options["log_level"].nil?)
#      min = Log::INFO
#    else
#      min = Log.get_by_name(@options["log_level"])
#    end
#
#    if(level >= min)
#      # TODO: @command is occasionally nil here - consider creating it earlier?
#      if(@command.nil?)
#        puts("[[#{Log::LEVELS[level]}]] :: #{message}")
#      else
#        @command.error("[[#{Log::LEVELS[level]}]] :: #{message}")
#      end
#    end
#  end

  def wakeup()
    @thread.raise(UiWakeup)
  end
end

