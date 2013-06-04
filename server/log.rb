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
  @@mutex = Mutex.new()

  INFO    = 0
  WARNING = 1
  ERROR   = 2
  FATAL   = 3

  @@min_level      = INFO
  @@min_file_level = INFO
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
    Log.log(INFO, message)
  end

  def Log.WARNING(message)
    Log.log(WARNING, message)
  end

  def Log.ERROR(message)
    Log.log(ERROR, message)
  end

  def Log.FATAL(message)
    Log.log(FATAL, message)
  end

  private
  def Log.log(level, message)
    if(message.is_a?(Array))
      message.each do |m|
        Log.log(level, m)
      end
    else
      if(level < INFO || level > FATAL)
        raise("Bad log level: #{level}")
      end

      @@mutex.synchronize do
        if(level >= @@min_level)
          $stderr.puts("[[ #{LEVELS[level]} ]] :: #{message}")
        end

        if(!@@file.nil? && level >= @@min_file_level)
          @@file.puts("[[ #{LEVELS[level]} ]] :: #{message}")
        end
      end
    end
  end
end

