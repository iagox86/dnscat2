##
# ui.rb
# Created June 20, 2013
# By Ron Bowes
##

require 'trollop' # We use this to parse commands
require 'readline' # For i/o operations
require 'ui_command'
require 'ui_session'

# Notification functions that are tied to a particular session:
# - session_created(id)
# - session_established(id)
# - session_data_received(id, data)
# - session_data_sent(id, data)
# - session_data_acknowledged(id, data)
# - session_data_queued(id, data)
# - session_destroyed(id)
# - session_heartbeat(id)
#
# Calls that aren't tied to a session:
# - dnscat2_syn_received(my_seq, their_seq)
# - dnscat2_msg_bad_seq(expected_seq, received_seq)
# - dnscat2_msg_bad_ack(expected_ack, received_ack)
# - dnscat2_msg(incoming, outgoing)
# - dnscat2_fin()
# - dnscat2_recv(packet)
# - dnscat2_send(packet)

# Hacks that I'm not proud of:
# - To store multiple 'HISTORY' buffers on readline, we have to modify the
#   contents of the Readline::HISTORY constant multiple times
# - To break out of Readline.readline(), we define an exception and throw it
#   at the thread whenever we need to break out
# - To break out cleanly, we also have to send a signal, so we're using USR1
#   and trapping it to do nothing
#
# I'm not proud of these, but they seem to work well and they avoid me having
# to implement my own i/o operations from scratch or make the user experience
# suffer.

class Ui
  @@options = {}
  @@thread = Thread.current()

  @@ui_command = UiCommand.new()

  # The current session we're interacting with, or nil
  @@session = nil

  # The sessions are indexed using the local_id, not the actual session_id!!
  @@sessions = {}

  @@local_ids = {}

  # My own history buffers - see the 'hacks' section in the file comment for
  # more information
  @@command_history = []
  @@session_history = {}

  class UiWakeup < Exception
    # Nothing required
  end

  def Ui.set_option(name, value)
    # Remove whitespace
    name  = name.to_s
    value = value.to_s

    name   = name.gsub(/^ */, '').gsub(/ *$/, '')
    value = value.gsub(/^ */, '').gsub(/ *$/, '')

    if(value == "nil")
      @@options.delete(name)

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

      @@options[name] = value

      puts("#{name} => #{value}")
    end
  end

  def Ui.each_option()
    @@options.each_pair do |k, v|
      yield(k, v)
    end
  end

  def Ui.get_option(name)
    return @@options[name]
  end

  def Ui.error(msg)
    $stderr.puts("ERROR: #{msg}")
  end

  def Ui.save_history(local_id)
    array = nil
    if(local_id.nil?)
      array = @@command_history
    else
      array = @@session_history[local_id]
    end

    Readline::HISTORY.each do |i|
      array << i
    end
  end

  def Ui.restore_history(local_id)
    Readline::HISTORY.clear()

    if(local_id.nil?)
      @@command_history.each do |i|
        Readline::HISTORY << i
      end
    else
      @@session_history[local_id].each do |i|
        Readline::HISTORY << i
      end
    end
  end

  def Ui.switch_history(from, to)
    save_history(from)
    restore_history(to)
  end

  def Ui.attach_session(local_id)
    if(@@sessions[local_id].nil?)
      Ui.error("Unknown session: #{local_id}")
    else
      session = @@sessions[local_id]
      if(session.attach)
        Ui.switch_history((@@session.nil? ? nil : local_id), local_id)
        @@session = session
      end
    end
  end

  def Ui.detach_session(local_id = nil)
    if(@@session.nil?)
      return
    end

    # TODO: What does this do, and can I make it simpler?
    if(local_id.nil? || (@@session.local_id == local_id))
      Ui.switch_history((@@session.nil? ? nil : @@session.local_id), nil)
      @@session.detach
      @@session = nil
    end
  end

  def Ui.get_ui_session(local_id)
    session = @@sessions[local_id]
    if(session.nil?)
      Ui.error("Unknown session: #{local_id}")
      return nil
    end
    if(!session.active?)
      Ui.error("Inactive session: #{local_id}")
      return nil
    end

    return session
  end

  # Map a real_id to a local_id
  def Ui.create_local_id(real_id)
    @@local_ids[real_id] = @@local_ids.size + 1 # +1 so it starts at 1, not 0
    return @@local_ids[real_id]
  end

  def Ui.go()
    # Ensure that USR1 does nothing, see the 'hacks' section in the file
    # comment
    Signal.trap("USR1") do
      # Do nothing
    end

    loop do
      begin
        # Verify that @@session is still active?
        if(!@@session.nil? && !@@session.active?)
          Ui.detach(@@session.id)
        end

        # Call the appropriate "go" function
        if(@@session.nil?)
          @@ui_command.go()
        else
          @@session.go()
        end

      # Capture the UiWakeup exception, see the 'hacks' section in the file
      # comment
      rescue UiWakeup
        #puts("Woken up!")
        puts()
      end
    end
  end

  def Ui.each_session()
    @@sessions.each do |s|
      yield(s)
    end
  end

  #################
  # The rest of this are callbacks
  #################

  def Ui.session_created(real_id)
    # Don't really care until the session is established
  end

  def Ui.session_established(real_id)
    local_id = Ui.create_local_id(real_id)

    puts("New session established: #{local_id}")
    @@sessions[local_id] = UiSession.new(local_id, SessionManager.find(real_id))
    @@session_history[local_id] = []

    # If no session is currently attached and we're auto-attaching sessions,
    # attach it then trigger a wakeup
    if(@@options['auto_attach'] == true && @@session.nil?)
      Ui.attach_session(local_id)
      Ui.wakeup()
    end
  end

  def Ui.session_data_received(real_id, data)
    local_id = @@local_ids[real_id]
    session = Ui.get_ui_session(local_id)
    if(!session.nil?)
      session.data_received(data)
    end
  end

  def Ui.session_data_sent(real_id, data)
    #local_id = @@local_ids[real_id]
  end

  def Ui.session_data_acknowledged(real_id, data)
    local_id = @@local_ids[real_id]
    session = Ui.get_ui_session(local_id)
    if(!session.nil?)
      session.data_acknowledged(data)
    end
  end

  def Ui.session_data_queued(real_id, data)
    #local_id = @@local_ids[real_id]
  end

  def Ui.session_destroyed(real_id)
    local_id = @@local_ids[real_id]

    # If the session is attached, detach it
    if(!@@session.nil? && @@session.local_id == local_id)
      Ui.detach_session(local_id)
    end

    # If the session exists, kill it
    if(!@@sessions[local_id].nil?)
      @@sessions[local_id].set_state("closed")
      #@@sessions[local_id].destroy
      #@@sessions.delete(local_id)
      #@@session_history.delete(local_id)
    end

    # Make sure the UI is updated
    Ui.wakeup()
  end

  def Ui.session_heartbeat(real_id)
    local_id = @@local_ids[real_id]

    session = Ui.get_ui_session(local_id)
    if(!session.nil?)
      session.heartbeat()
    end
  end

  def Ui.dnscat2_state_error(real_id, message)
    local_id = @@local_ids[real_id]
    Ui.error("#{message} :: Session: #{local_id}")
  end

  def Ui.dnscat2_syn_received(real_id, my_seq, their_seq)
  end

  def Ui.dnscat2_msg_bad_seq(expected_seq, received_seq)
  end

  def Ui.dnscat2_msg_bad_ack(expected_ack, received_ack)
    Ui.error("WARNING: Impossible ACK received: 0x%04x, current SEQ is 0x%04x" % [received_ack, expected_ack])
  end

  def Ui.dnscat2_msg(incoming, outgoing)
  end

  def Ui.dnscat2_fin(real_id)
    # Ui.session_destroyed() will take care of this
  end

  # TODO: This doesn't work because I got rid of the send/recv messages
  def Ui.dnscat2_recv(packet)
    if(@@options["packet_trace"])
      puts("IN: #{packet}")
    end
  end

  def Ui.dnscat2_send(packet)
    if(@@options["packet_trace"])
      puts("OUT: #{packet}")
    end
  end

  def Ui.log(level, message)
    # Handle the special case, before a level is set
    if(@@options["log_level"].nil?)
      min = Log::INFO
    else
      min = Log.get_by_name(@@options["log_level"])
    end

    if(level >= min)
      puts("[[#{Log::LEVELS[level]}]] :: #{message}")
    end
  end

  def Ui.wakeup()
    @@thread.raise(UiWakeup)

    if(@@options["signals"])
      # A signal has to be sent to wake up the thread, otherwise it waits for
      # user input
      Process.kill("USR1", 0)
    end
  end
end

