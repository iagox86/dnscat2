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

require 'ui'

class Log
  @@mutex = Mutex.new()

  # Begin subscriber stuff (this should be in a mixin, but static stuff doesn't
  # really seem to work
  @@subscribers = []
  def Log.subscribe(cls)
    @@subscribers << cls
  end
  def Log.unsubscribe(cls)
    @@subscribers.delete(cls)
  end
  def Log.notify_subscribers(method, args)
    @@subscribers.each do |subscriber|
      if(subscriber.respond_to?(method))
         subscriber.method(method).call(*args)
      end
    end
  end
  # End subscriber stuff

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

  def Log.get_by_name(name)
    return LEVELS_BY_NAME[name.upcase]
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
        raise(RuntimeError, "Bad log level: #{level}")
      end

      @@mutex.synchronize do
        Log.notify_subscribers(:log, [level, message])
      end
    end
  end
end

