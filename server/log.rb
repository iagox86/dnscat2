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
  LOG_INFO    = 0
  LOG_WARNING = 1
  LOG_ERROR   = 2
  LOG_FATAL   = 3

  @@min_level      = LOG_INFO
  @@min_file_level = LOG_INFO
  @@file           = nil

  LEVELS = [ "INFO", "WARNING", "ERROR", "FATAL" ]

  def Log.set_min_level(min_level)
    @@min_level = min_level
  end

  def Log.set_file(file, min_level)
    @@file = File.open(file, "w")
    @@min_file_level = min_level
  end

  def Log.INFO(message)
    Log.log(LOG_INFO, message)
  end

  def Log.WARNING(message)
    Log.log(LOG_WARNING, message)
  end

  def Log.ERROR(message)
    Log.log(LOG_ERROR, message)
  end

  def Log.FATAL(message)
    Log.log(LOG_FATAL, message)
  end

  private
  def Log.log(level, message)
    if(message.is_a?(Array))
      message.each do |m|
        Log.log(level, m)
      end
    else
      if(level < LOG_INFO || level > LOG_FATAL)
        raise("Bad log level: #{level}")
      end

      if(level >= @@min_level)
        $stderr.puts("[[ #{LEVELS[level]} ]] :: #{message}")
      end

      if(!@@file.nil? && level >= @@min_file_level)
        @@file.puts("[[ #{LEVELS[level]} ]] :: #{message}")
      end
    end
  end
end

