##
# settings.rb
# By Ron Bowes
# February 8, 2014
#
# See LICENSE.md
#
# This is a class for managing ephemeral settings for a project.
#
# When the program starts, any number of settings can be registered either for
# individually created instances of this class, or for a global instances -
# Settings::GLOBAL - that is automatically created (and that is used for
# getting/setting settings that don't exist).
#
# What makes this more useful than a hash is two things:
#
# 1. Mutators - Each setting can have a mutator (which is built-in and related
# to the type) that can alter it. For example, TYPE_BOOLEAN has a mutator that
# changes 't', 'true', 'y', 'yes', etc to true. TYPE_INTEGER converts to a
# proper number (or throws an error if it's not a number), and so on.
#
# 2. Validators/callbacks - When a setting is defined, it's given a block that
# executes whenever the value changes. That block can either prevent the change
# by raising a Settings::ValidationError (which the program has to catch), or
# can process the change in some way (by, for example, changing a global
# variable).
#
# Together, those features make this class fairly flexible and useful!
##

class Settings
  def initialize()
    @settings = {}
  end

  GLOBAL = Settings.new()

  class ValidationError < StandardError
  end

  TYPE_STRING       = 0
  TYPE_INTEGER      = 1
  TYPE_BOOLEAN      = 2
  TYPE_BLANK_IS_NIL = 3
  TYPE_NO_STRIP     = 4

  @@mutators = {
    TYPE_STRING => Proc.new() do |value|
      value.strip()
    end,

    TYPE_INTEGER => Proc.new() do |value|
      if(value.nil?)
        raise(Settings::ValidationError, "Can't be nil!")
      end
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

  # Set the name to the new value. The name has to have previously been defined
  # by calling the create() function.
  #
  # If this isn't Settings::GLOBAL and allow_recursion is set, unrecognized
  # variables will be retrieved, if possible, from Settings::GLOBAL.
  def set(name, new_value, allow_recursion=true)
    name = name.to_s()
    new_value = new_value.to_s()

    if(@settings[name].nil?)
      if(!allow_recursion)
        raise(Settings::ValidationError, "No such setting!")
      end

      return Settings::GLOBAL.set(name, new_value, false)
    end

    old_value = @settings[name][:value]
    new_value = @@mutators[@settings[name][:type]].call(new_value)

    if(@settings[name][:watcher] && old_value != new_value)
      @settings[name][:watcher].call(old_value, new_value)
    end

    @settings[name][:value] = new_value

    return old_value
  end

  # Set a variable back to the default value.
  def unset(name, allow_recursion=true)
    if(@settings[name].nil?)
      if(!allow_recursion)
        raise(Settings::ValidationError, "No such setting!")
      end

      return Settings::GLOBAL.unset(name)
    end

    set(name, @settings[name][:default].to_s(), allow_recursion)
  end

  # Get the current value of a variable.
  def get(name, allow_recursion=true)
    name = name.to_s()

    if(@settings[name].nil?)
      if(allow_recursion)
        return GLOBAL.get(name, false)
      end
    end

    return @settings[name][:value]
  end

  # Yields for each setting. Each setting has a name, a value, a documentation
  # string, and a default value.
  def each_setting()
    @settings.each_pair do |k, v|
      yield(k, v[:value], v[:docs], v[:default])
    end
  end

  # Create a new setting, or replace an old one. This must be done before a
  # setting is used.
  def create(name, type, default_value, docs)
    name = name.to_s()

    @settings[name] = @settings[name] || {}

    @settings[name][:type]    = type
    @settings[name][:watcher] = proc
    @settings[name][:docs]    = docs
    @settings[name][:default] = @@mutators[type].call(default_value.to_s())

    # This sets it to the default value
    unset(name, false)
  end

  # Replaces any variable found in the given, in the form '$var' where 'var'
  # is a setting, with the setting value
  #
  # For example if "id" is set to 123, then the string "the id is $id" will
  # become "the id is 123".
  def do_replace(str, allow_recursion = true)
    @settings.each_pair do |name, setting|
      str = str.gsub(/\$#{name}/, setting[:value].to_s())
    end

    if(allow_recursion)
      str = Settings::GLOBAL.do_replace(str, false)
    end

    return str
  end
end
