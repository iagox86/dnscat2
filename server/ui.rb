##
# ui.rb
# Created June 20, 2013
# By Ron Bowes
##

require 'trollop' # We use this to parse commands
require 'readline' # For i/o operations
require 'ui_command'
require 'ui_session'
#require 'ui_session_command'

class Ui
  def initialize()
    @options = {}
    @thread = Thread.current()

    # There's always a single UiCommand in existance
    @command = nil

    # This is a handle to the current UI the user is interacting with
    @ui = nil

    # The current local_id
    @current_local_id = 0

    # This is a list of all UIs that are available, indexed by local_id
    @uis_by_local_id = {}
    @uis_by_real_id = {}

    # A mapping of real ids to session ids
    @id_map = {}
  end

  # TODO: Is this still necessary?
  class UiWakeup < Exception
    # Nothing required
  end

  def get_by_local_id(id)
    return @uis_by_local_id[id]
  end

  def get_by_real_id(id)
    return @uis_by_real_id[id]
  end

  def set_option(name, value)
    # Remove whitespace
    name  = name.to_s
    value = value.to_s

    name   = name.gsub(/^ */, '').gsub(/ *$/, '')
    value = value.gsub(/^ */, '').gsub(/ *$/, '')

    if(value == "nil")
      @options.delete(name)

      puts("#{name} => [deleted]")
    else

      # Replace \n with actual newlines
      value = value.gsub(/\\n/, "\n")

      # Replace true/false with the proper values
      value = true if(value == "true")
      value = false if(value == "false")

      # Validate the log level
      if(name == "log_level" && Log.get_by_name(value).nil?)
        puts("ERROR: Legal values for log_level are: #{Log::LEVELS}")
        return
      end

      @options[name] = value

      puts("#{name} => #{value}")
    end
  end

  def each_option()
    @options.each_pair do |k, v|
      yield(k, v)
    end
  end

  def get_option(name)
    return @options[name]
  end

  # TODO: Make this display the error to the proper session or the command window
  def error(msg, local_id = nil)
    # Try to use the provided id first
    if(!local_id.nil?)
      ui = @uis_by_local_id[local_id]
      if(!ui.nil?)
        ui.error(msg)
        return
      end
    end

    # As a fall-back, or if the local_id wasn't provided, output to the current or
    # the command window
    if(@ui.nil?)
      @command.error(msg)
    else
      @ui.error(msg)
    end
  end

  # Detach the current session and attach a new one
  def attach_session(ui = nil)
    # Detach the old ui
    if(!@ui.nil?)
      @ui.detach()
    end

    # By default, attach to the command window
    if(ui.nil?)
      ui = @command
    end

    # Go to the new ui
    @ui = ui

    # Attach the new ui
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
          Log.ERROR("@ui ended up nil somehow!")
        end

        # If the ui is no longer active, switch to the @command window
        if(!@ui.active?)
          @ui.error("UI went away...")
          attach(@command)
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
    @uis_by_local_id.each do |s|
      yield(s)
    end
  end

  #################
  # The rest of this are callbacks
  #################

  def session_established(real_id)
    # Generate the local id
    local_id = @current_local_id + 1
    @current_local_id += 1

    # Create the mapping
    @id_map[real_id] = local_id

    # Get a handle to the session
    session = SessionManager.find(real_id)

    # Fail if it doesn't exist
    if(session.nil?)
      raise(DnscatException, "Couldn't find the new session!")
    end

    # Create a new UI
    # TODO: This needs to be different depending on the session type... dunno how I'm gonna figure that out, though
    ui = UiSession.new(local_id, session, self)

    # Save it in both important lists
    @uis_by_local_id[local_id] = ui
    @uis_by_real_id[real_id]   = ui

    # Tell the command window
    @command.output("New session established: #{local_id}")
  end

  def session_data_received(real_id, data)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end
    ui.feed(data)
  end

  def session_data_acknowledged(real_id, data)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end
    ui.ack(data)
  end

  def session_destroyed(real_id)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end

    # Tell the UI it's been destroyed
    ui.destroy()

    # Switch the session for @command if it's attached
    if(@ui == ui)
      # Switch to the command window
      attach_session(nil)
    end

    # Make sure the UI is updated
    wakeup()
  end

  # TODO: Not sure that this is needed at this level?
#  def kill_session(local_id)
#    session = @sessions[local_id]
#    if(session.nil?())
#      return false
#    end
#
#    session.session.kill()
#
#    return true
#  end

  def session_heartbeat(real_id)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end
    ui.heartbeat()
  end

  def dnscat2_state_error(real_id, message)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end
    ui.error(message)
  end

  def dnscat2_syn_received(real_id, my_seq, their_seq)
  end

  def dnscat2_msg_bad_seq(expected_seq, received_seq)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end
    ui.error("Bad sequence number; expected 0x%04x, received 0x%04x" % received_seq, expected_seq)
  end

  def dnscat2_msg_bad_ack(expected_ack, received_ack)
    ui = @uis_by_real_id[real_id]
    if(ui.nil?)
      raise(DnscatException, "Couldn't find session: #{real_id}")
    end
    ui.error("Bad acknowledgement number; expected 0x%04x, received 0x%04x" % received_ack, expected_ack)
  end

  def dnscat2_msg(incoming, outgoing)
  end

  def dnscat2_fin(real_id, reason)
#    # session_destroyed() will take care of this
#    local_id = @local_ids[real_id]
#
#    session = get_ui_session(local_id)
#    session.display("Session terminated: %s" % reason, '[ERROR]')
#    SessionManager.kill_session(real_id)
  end

  # TODO: This doesn't work because I got rid of the send/recv messages
#  def dnscat2_recv(packet)
#    if(@options["packet_trace"])
#      puts("IN: #{packet}")
#    end
#  end
#
#  def dnscat2_send(packet)
#    if(@options["packet_trace"])
#      puts("OUT: #{packet}")
#    end
#  end

  def log(level, message)
    # Handle the special case, before a level is set
    if(@options["log_level"].nil?)
      min = Log::INFO
    else
      min = Log.get_by_name(@options["log_level"])
    end

    if(level >= min)
      # TODO: @command is occasionally nil here - consider creating it earlier?
      if(@command.nil?)
        puts("[[#{Log::LEVELS[level]}]] :: #{message}")
      else
        @command.error("[[#{Log::LEVELS[level]}]] :: #{message}")
      end
    end
  end

  def wakeup()
    @thread.raise(UiWakeup)
  end
end

