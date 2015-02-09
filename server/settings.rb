# settings.rb
# By Ron Bowes
# February 8, 2014

class Settings
  def initialize(settings = {})
    @settings = settings
    @watchers = {}
    @verifiers = {}
  end

  def set(name, value)
    (@verifiers[name] || []).each do |verifier|
      result = verifier.call(value)
      if(result)
        return result
      end
    end

    (@watchers[name] || []).each do |callback|
      callback.call(@settings[name], value)
    end

    if(value.nil?)
      @settings.delete(name)
    else
      @settings[name] = value
    end

    return nil
  end

  def get(name)
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
    @watchers[name] ||= []
    @watchers[name] << proc
  end

  def verify(name)
    @verifiers[name] ||= []
    @verifiers[name] << proc
  end
end
