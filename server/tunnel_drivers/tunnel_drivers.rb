##
# tunnel_drivers.rb
# Created September 17, 2015
# By Ron Bowes
#
# See: LICENSE.md
##

class TunnelDrivers
  @@drivers = {}

  class StopDriver < Exception
  end

  def TunnelDrivers.start(params = {})
    controller    = params[:controller] || raise(ArgumentError, "The :controller argument is required")
    parent_window = params[:window]     || raise(ArgumentError, "The :window argument is required")
    driver_cls    = params[:driver]     || raise(ArgumentError, "The :driver argument is required")
    args          = params[:args]       || raise(ArgumentError, "The :args argument is required")
    if(!args.is_a?(Array))
      raise(ArgumentError, "The :args argument must be an array")
    end

    begin
      driver = driver_cls.new(parent_window, *args) do |data, max_length|
        controller.feed(data, max_length)
      end
      @@drivers[driver.id] = driver
    rescue Errno::EACCES => e
      controller.window.with({:to_ancestors => true}) do
        controller.window.puts("*** ERROR ***")
        controller.window.puts("")
        controller.window.puts("There was a problem creating the socket: #{e}")
        controller.window.puts("")
        controller.window.puts("If you're trying to run this on Linux, chances are you need to")
        controller.window.puts("run this as root, or give ruby permission to listen on port 53.")
        controller.window.puts("")
        controller.window.puts("Sadly, this is non-trivial; rvmsudo doesn't work, because it's")
        controller.window.puts("a shellscript and breaks ctrl-z; the best way is to use 'su' or")
        controller.window.puts("'sudo', and to ensure that the appropriate gems are globally")
        controller.window.puts("installed.")
      end
    end
  end

  def TunnelDrivers.exists?(id)
    return !@@drivers[id].nil?
  end

  def TunnelDrivers.stop(id)
    driver = @@drivers[id]

    if(!driver)
      return false
    end

    driver.stop()
    @@drivers.delete(id)
  end
end
