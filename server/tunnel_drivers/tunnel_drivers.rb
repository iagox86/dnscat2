##
# tunnel_drivers.rb
# Created September 17, 2015
# By Ron Bowes
#
# See: LICENSE.md
##

class TunnelDrivers
  @@drivers = {}
  @@id = 0

  class StopDriver < Exception
  end

  def TunnelDrivers.start(controller, driver)
    begin
      driver.start() do |data, max_length|
        controller.feed(data, max_length)
      end
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
#    @@drivers[driver.id] = {
#      :driver => driver,
#      :thread => Thread.new do |t|
#        begin
#          driver.recv() do |data, max_length|
#            controller.feed(data, max_length)
#          end
#        rescue TunnelDrivers::StopDriver => e
#          driver.window.puts("Stopping this tunnel: #{e}")
#        rescue Exception => e
#          driver.window.puts("Error in the TunnelDriver: #{e.inspect}")
#          e.backtrace.each do |b|
#            driver.window.puts("#{b}")
#          end
#        end
#      end
#    }
  end

  def TunnelDrivers.exists?(id)
    return !@@drivers[id].nil?
  end

  def TunnelDrivers.stop(id)
    driver = @@drivers[id]

    if(!driver)
      return false
    end

    driver[:driver].stop()
    driver[:thread].raise(TunnelDrivers::StopDriver, "Close!")
    driver.delete(id)
  end
end
