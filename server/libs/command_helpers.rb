##
# command_helpers.rb
# Created September 16, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
# These are used both by controller_commands.rb and driver_command_commands.rb,
# so instead of copying the code I put them together.
#
# It's unlikely that anybody else would have a use for this.
##

class CommandHelpers
  # Displays the name and children of a window
  def CommandHelpers._display_window(window, all, stream, indent = 0)
    if(!all && window.closed?())
      return
    end

    stream.puts(('  ' * indent) + window.to_s())
    window.children() do |c|
      _display_window(c, all, stream, indent + 1)
    end
  end

  def CommandHelpers.display_windows(base_window, all, stream)
    _display_window(base_window, all, stream)
  end

  def CommandHelpers.wrap(s, width=72, indent=0)
    return s.gsub(/(.{1,#{width}})(\s+|\Z)/, "#{" "*indent}\\1\n")
  end

  def CommandHelpers.format_field(s)
    if(s.nil?)
      return "(n/a)"
    elsif(s.is_a?(String))
      return "'" + s + "'"
    else
      return s.to_s()
    end
  end

  def CommandHelpers.parse_setting_string(str, defaults = nil)
    response = (defaults || {}).clone()

    str.split(/,/).each do |segment|
      name, value = segment.split(/=/, 2)
      if(value.nil?)
        raise(ArgumentError, "Invalid settings string; a comma-separated list of name=value pairs is required. ('#{str}')")
      end

      name = name.to_sym()
      if(defaults && !defaults.has_key?(name))
        raise(ArgumentError, "Invalid setting: #{name}; allowed settings are #{defaults.keys.join(', ')}. ('#{str}')")
      end

      if(response[name].is_a?(Array))
        response[name] << value
      else
        response[name] = value
      end
    end

    return response
  end
end
