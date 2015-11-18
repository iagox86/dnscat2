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
    driver_cls    = params[:driver]     || raise(ArgumentError, "The :driver argument is required")
    args          = params[:args]       || raise(ArgumentError, "The :args argument is required")
    if(!args.is_a?(Array))
      raise(ArgumentError, "The :args argument must be an array")
    end

    begin
      driver = driver_cls.new(WINDOW, *args) do |data, max_length|
        controller.feed(data, max_length)
      end
      @@drivers[driver.id] = driver
    rescue Errno::EACCES => e
        puts("")
        puts("*** ERROR")
        puts("*")
        puts("* There was a problem creating the socket: #{e}")
        puts("*")
        puts("* If you're trying to run this on Linux, chances are you need to")
        puts("* run this as root, or give ruby permission to listen on port 53.")
        puts("*")
        puts("* Sadly, this is non-trivial; rvmsudo doesn't work, because it's")
        puts("* a shellscript and breaks ctrl-z; the best way is to use 'su' or")
        puts("* 'sudo', and to ensure that the appropriate gems are globally")
        puts("* installed.")
        puts("*")
        puts("* The process will run as usual, but if the 'windows' command doesn't")
        puts("* show any listeners, nobody will be able to connect to you!")
        puts("*")
        puts("*** ERROR")
        puts("")
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

  def TunnelDrivers.each_driver()
    @@drivers.each_pair do |id, driver|
      yield(id, driver)
    end
  end
end
