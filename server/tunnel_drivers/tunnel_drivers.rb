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
      controller.window.puts_ex("*** ERROR ***", {:to_ancestors => true})
      controller.window.puts_ex("", {:to_ancestors => true})
      controller.window.puts_ex("There was a problem creating the socket: #{e}", {:to_ancestors => true})
      controller.window.puts_ex("", {:to_ancestors => true})
      controller.window.puts_ex("If you're trying to run this on Linux, chances are you need to", {:to_ancestors => true})
      controller.window.puts_ex("run this as root, or give ruby permission to listen on port 53.", {:to_ancestors => true})
      controller.window.puts_ex("", {:to_ancestors => true})
      controller.window.puts_ex("Sadly, this is non-trivial; rvmsudo doesn't work, because it's", {:to_ancestors => true})
      controller.window.puts_ex("a shellscript and breaks ctrl-z; the best way is to use 'su' or", {:to_ancestors => true})
      controller.window.puts_ex("'sudo', and to ensure that the appropriate gems are globally", {:to_ancestors => true})
      controller.window.puts_ex("installed.", {:to_ancestors => true})
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
