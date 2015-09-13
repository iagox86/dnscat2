# settings.rb
# By Ron Bowes
# February 8, 2014
#
# See LICENSE.md

require 'libs/log'

class Settings
  def initialize()
    @settings = {}
  end

  GLOBAL = Settings.new()

  class ValidationError < StandardError
  end

  TYPE_STRING = 0
  TYPE_INTEGER = 1
  TYPE_BOOLEAN = 2
  TYPE_BLANK_IS_NIL = 3
  TYPE_NO_STRIP = 4

  @@mutators = {
    TYPE_STRING => Proc.new() do |value|
      value.strip()
    end,

    TYPE_INTEGER => Proc.new() do |value|
      if(value.start_with?('0x'))
        if(value[2..-1] !~ /^[\h]+$/)
          raise(Settings::ValidationError, "Not a value hex string: #{value}")
        end

        value[2..-1].to_i(16)
      else
        if(value !~ /^[\d]+$/)
          raise(Settings::ValidationError, "Not a valid number: #{value}")
        end

        value.to_i()
      end
    end,

    TYPE_BOOLEAN => Proc.new() do |value|
      value = value.downcase()

      if(['t', 1, 'y', 'true', 'yes'].index(value))
        # return
        true
      elsif(['f', 0, 'n', 'false', 'no'].index(value))
        # return
        false
      else
        raise(Settings::ValidationError, "Expected: true/false")
      end

    end,

    TYPE_BLANK_IS_NIL => Proc.new() do |value|
      value == '' ? nil : value.strip()
    end,

    TYPE_NO_STRIP => Proc.new() do |value|
      value
    end,
  }

  def set(name, new_value, allow_recursion = true)
    name = name.to_s()

    if(@settings[name].nil?)
      if(!allow_recursion)
        raise(Settings::ValidationError, "No such setting!")
      end

      return Settings::GLOBAL.set(name, new_value, false)
    end

    old_value = @settings[name][:value]
    new_value = @@mutators[@settings[name][:type]].call(new_value)

    if(@settings[name][:watcher])
      @settings[name][:watcher].call(old_value, new_value)
    end

    @settings[name][:value] = new_value

    return old_value
  end

  def get(name, allow_recursion = true)
    name = name.to_s()

    if(@settings[name].nil?)
      if(allow_recursion)
        return GLOBAL.get(name, false)
      end
    end

    return @settings[name][:value]
  end

  def keys()
    return @settings.keys
  end

  def each_pair()
    @settings.each_pair do |k, v|
      yield(k, v[:value])
    end
  end

  def create(name, type, default_value)
    name = name.to_s()

    @settings[name] = @settings[name] || {}

    @settings[name][:type]    = type
    @settings[name][:watcher] = proc

    set(name, default_value, false)
  end
end
