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
  def CommandHelpers._display_window(window, all, indent = 0)
    if(!all && window.closed?())
      return
    end

    @window.puts(('  ' * indent) + window.to_s())
    window.children() do |c|
      _display_window(c, all, indent + 1)
    end
  end

  def CommandHelpers.display_windows(all)
    _display_window(@window, all)
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
end
