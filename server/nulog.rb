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

class NuLog
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
  # NuLog.logging(id) do |msg|
  #   puts(msg)
  # end
  #
  # Choose an id of nil to get any logs that don't have a "home"
  # Choose NuLog::LOG_ALL_THE_THINGS for a catch-all address
  def NuLog.logging(id)
    if(id.is_a?(Fixnum) || id.nil?() || id == LOG_ALL_THE_THINGS)
      @@loggers[id] ||= []
      @@loggers[id] << proc
    else
      raise(DnscatException, "Failed to add a logger for id #{id} - wrong type")
    end
  end

  def NuLog.set_min_level(level)
    @@min_level = level
  end

  def NuLog.get_level_by_name(name)
    return LEVELS_BY_NAME[name.upcase]
  end

  def NuLog.INFO(message, id = nil)
    NuLog.log(INFO, message, id)
  end

  def NuLog.WARNING(message, id = nil)
    NuLog.log(WARNING, message, id)
  end

  def NuLog.ERROR(message, id = nil)
    NuLog.log(ERROR, message, id)
  end

  def NuLog.FATAL(message, id = nil)
    NuLog.log(FATAL, message, id)
  end

  def NuLog.try_to_log_to(message, id)
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

  def NuLog.log(level, message, id = nil)
    if(level < INFO || level > FATAL)
      raise(DnscatException, "Bad log level: #{level}")
    end

    if(level < @@min_level)
      return
    end

    message = "[%s] %s" % [(LEVELS[level] || "[bad level]"), message]

    NuLog.try_to_log_to(message, LOG_ALL_THE_THINGS)

    if(!NuLog.try_to_log_to(message, id))
      if(!NuLog.try_to_log_to("DEFAULT: " + message, nil))
        puts("Couldn't find anywhere to log message with id #{id}: #{message}")
      end
    end
  end
end

