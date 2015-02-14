# settings.rb
# By Ron Bowes
# February 8, 2014

require 'log'

class Settings
  def initialize(settings = {})
    @settings = settings
    @watchers = {}
    @verifiers = {}
  end

  def set(name, value)
    name = name.to_s()
    (@verifiers[name] || []).each do |verifier|
      result = verifier.call(value)
      if(result)
        return result
      end
    end

    (@watchers[name] || []).each do |callback|
      result = callback.call(@settings[name], value)
      if(!result.nil?)
        Log.ERROR(nil, "Couldn't change setting: #{result}")
        return "error"
      end
    end

    if(value.nil?)
      @settings.delete(name)
    else
      @settings[name] = value
    end

    return nil
  end

  def get(name)
    name = name.to_s()
    return @settings[name]
  end

  def keys()
    return @settings.keys
  end

  def each_pair()
    @settings.each_pair do |k, v|
      yield(k, v)
    end
  end

  def watch(name)
    name = name.to_s()
    @watchers[name] ||= []
    @watchers[name] << proc
  end

  def verify(name)
    name = name.to_s()
    @verifiers[name] ||= []
    @verifiers[name] << proc
  end

  def print()
    @settings.each_pair do |k, v|
      puts(k.to_s + " => " + v.to_s)
    end
  end
end
