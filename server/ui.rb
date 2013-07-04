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
#
# Calls that aren't tied to a session:
# - dnscat2_syn_received(my_seq, their_seq)
# - dnscat2_msg_bad_seq(expected_seq, received_seq)
# - dnscat2_msg_bad_ack(expected_ack, received_ack)
# - dnscat2_msg(incoming, outgoing)
# - dnscat2_fin()
# - dnscat2_recv(packet)
# - dnscat2_send(packet)

class Ui
  @@options = {}

  @@ui_command = UiCommand.new()
  @@sessions = {}
  @@session = nil

  # TODO: Handle options in a more structured way
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

  def Ui.create_session(session_id)
    # TODO: Pass in the options array?
    @@sessions[session_id] = ui_session.new(session_id)
  end

  def Ui.destroy_session(session_id)
    if(!@@session.nil? && @@session.id == session_id)
      Ui.detach(session_id)
    end
    if(!@@sessions[session_id].nil?)
      @@sessions[session_id].destroy
      @@sessions.delete(session_id)
    end
  end

  def Ui.attach_session(session_id)
    if(@@sessions[session_id].nil?)
      Ui.error("Unknown session: #{session_id}")
    else
      @@session = @@sessions[session_id]
      @@session.attach
    end
  end

  def Ui.detach_session(id = nil)
    if(@@session.nil?)
      return
    end

    if(id.nil? || (@@session.id == id))
      @@session.detach
      @@session = nil
    end
  end

  def Ui.get_session(id)
    session = @@sessions[id]
    if(session.nil?)
      Ui.error("Unknown session: #{id}")
      return nil
    end
    if(!session.active?)
      Ui.error("Inactive session: #{id}")
      return nil
    end

    return session
  end

  def Ui.go()
    loop do
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
    end
  end

  #################
  # The rest of this are callbacks
  #################

  def Ui.session_created(id)
    # Don't really care until the session is established
  end

  def Ui.session_established(id)
    puts("New session established: #{id}")
    @@sessions[id] = UiSession.new(id)

  end

  def Ui.session_data_received(id, data)
    session = Ui.get_session(id)
    if(!session.nil?)
      session.data_received(data)
    end
  end

  def Ui.session_data_sent(id, data)
  end

  def Ui.session_data_acknowledged(id, data)
    session = Ui.get_session(id)
    if(!session.nil?)
      session.data_acknowledged(data)
    end
  end

  def Ui.session_data_queued(id, data)
  end

  def Ui.session_destroyed(id)
    Ui.destroy_session(id)
  end

  def Ui.dnscat2_state_error(session_id, message)
    Ui.error("#{message} :: Session: #{session_id}")
  end

  def Ui.dnscat2_syn_received(session_id, my_seq, their_seq)
  end

  def Ui.dnscat2_msg_bad_seq(expected_seq, received_seq)
  end

  def Ui.dnscat2_msg_bad_ack(expected_ack, received_ack)
    Ui.error("WARNING: Impossible ACK received: 0x%04x, current SEQ is 0x%04x" % [received_ack, expected_ack])
  end

  def Ui.dnscat2_msg(incoming, outgoing)
  end

  def Ui.dnscat2_fin(session_id)
    Ui.session_destroy(session_id)
  end

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
    begin
      # Handle the special case, before a level is set
      if(@@options["log_level"].nil?)
        min = Log::INFO
      else
        min = Log.get_by_name(@@options["log_level"])
      end

      if(level >= min)
        puts("[[#{Log::LEVELS[level]}]] :: #{message}")
      end
    rescue Exception => e
      puts("Error in logging code: #{e}")
      exit
    end
  end

end

