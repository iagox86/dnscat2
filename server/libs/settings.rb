# settings.rb
# By Ron Bowes
# February 8, 2014
#
# See LICENSE.md

require 'libs/log'

class Settings
  GLOBAL = Settings.new()

  def initialize(parent = nil, settings = {})
    @settings = settings
    @parent = parent
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

  def get(name, allow_recursion = true)
    name = name.to_s()

    if(allow_recursion && @settings[name].nil? && !@parent.nil?)
      return @parent.get(name)
    end

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

  def on_change(name, watcher = nil, verifier = nil)
    name = name.to_s()

    @watchers[name] ||= []
    @watchers[name] << watcher

    @verifiers[name] ||= []
    if(!verifier.nil?)
      @verifiers[name] << verifier
    end
  end

  def print()
    @settings.each_pair do |k, v|
      puts(k.to_s + " => " + v.to_s)
    end
  end
end
