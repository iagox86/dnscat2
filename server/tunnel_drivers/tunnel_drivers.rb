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
    driver.start() do |data, max_length|
      controller.feed(data, max_length)
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
