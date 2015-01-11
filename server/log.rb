##
# log.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# A very simple logging class. May improve this later, but for now it can be
# as simple as necessary.
##

class Log
  INFO    = 0
  WARNING = 1
  ERROR   = 2
  FATAL   = 3

  LEVELS = [ "INFO", "WARNING", "ERROR", "FATAL" ]

  LEVELS_BY_NAME = {
    "INFO" => 0,
    "WARNING" => 1,
    "ERROR" => 2,
    "FATAL" => 3
  }
  @@loggers = {}
  @@min_level = LEVELS_BY_NAME["INFO"]

  LOG_ALL_THE_THINGS = "log_catchall"

  # Usage:
  # Log.logging(id) do |msg|
  #   puts(msg)
  # end
  #
  # Choose an id of nil to get any logs that don't have a "home"
  # Choose Log::LOG_ALL_THE_THINGS for a catch-all address
  def Log.logging(id)
    @@loggers[id] ||= []
    @@loggers[id] << proc
  end

  def Log.set_min_level(level)
    level = level.upcase()
    @@min_level = LEVELS_BY_NAME[level]

    if(@@min_level.nil?)
      return false
    end
    return true
  end

  def Log.get_level_by_name(name)
    return LEVELS_BY_NAME[name.upcase]
  end

  def Log.INFO(id, message = "")
    Log.log(INFO, id, message)
  end

  def Log.WARNING(id, message = "")
    Log.log(WARNING, id, message)
  end

  def Log.ERROR(id, message = "")
    Log.log(ERROR, id, message)
  end

  def Log.FATAL(id, message = "")
    Log.log(FATAL, id, message)
  end

  def Log.try_to_log_to(id, message = "")
    if(@@loggers[id].nil?)
      return false
    end

    if(@@loggers[id].length == 0)
      return false
    end

    @@loggers[id].each do |sub|
      sub.call(message)
      #sub.log(message)
    end

    return true
  end

  def Log.log(level, id, message = "")
    # Make sure the level is sane
    if(level < INFO || level > FATAL)
      raise(DnscatException, "Bad log level: #{level}")
    end

    # Check if we should even bother
    if(level < @@min_level)
      return
    end

    if(message.is_a?(Array))
      message.each do |m|
        log(level, id, m)
      end
    elsif(message.is_a?(Exception))
      log(level, id, "[%s]: %s" % [message.class.to_s(), message.to_s()])
      log(level, id, message.backtrace)
    else
      message = "[%s] %s" % [(LEVELS[level] || "[bad level]"), message]

      Log.try_to_log_to(LOG_ALL_THE_THINGS, message)

      if(!Log.try_to_log_to(id, message))
        if(!Log.try_to_log_to(nil, message))
          puts("[nowhere to log] #{message}")
          puts("Tried to log here: #{id}")
        end
      end
    end
  end

  def Log.reset()
    Log.INFO(nil, "Resetting the list of loggers")
    @@loggers.clear()
  end
end

