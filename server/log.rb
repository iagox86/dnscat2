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

require 'subscribable'
require 'ui'

class Log
  include Subscribable

  # For the singleton-style interface
  @@log = Log.new()

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

  def Log.subscribe(cls)
    Log.get_instance().subscribe(cls)
  end

  def Log.get_by_name(name)
    return LEVELS_BY_NAME[name.upcase]
  end

  def Log.INFO(message)
    Log.get_instance().log(INFO, message)
  end

  def Log.WARNING(message)
    Log.get_instance().log(WARNING, message)
  end

  def Log.ERROR(message)
    Log.get_instance().log(ERROR, message)
  end

  def Log.FATAL(message)
    Log.get_instance().log(FATAL, message)
  end

  def Log.get_instance()
    return @@log
  end

  def log(level, message)
    if(message.is_a?(Array))
      message.each do |m|
        log(level, m)
      end
    else
      if(level < INFO || level > FATAL)
        raise(RuntimeError, "Bad log level: #{level}")
      end

      notify_subscribers(:log, [level, message])
    end
  end

  private

  def initialize()

  end
end

