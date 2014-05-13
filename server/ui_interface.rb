##
# ui_interface.rb
# By Ron Bowes
# Created May, 2014
#
# See LICENSE.txt
##

class UiInterface
  def save_history()
    @history = []
    Readline::HISTORY.each do |i|
      @history << i
    end
  end
  def restore_history()
    @history = @history || []

    Readline::HISTORY.clear()
    @history.each do |i|
      Readline::HISTORY << i
    end
  end

  def feed(data)
    raise("Not implemented")
  end

  def ack(data)
    # Do nothing by default
  end

  def destroy()
    # Do nothing by default
  end

  def heartbeat()
    # Do nothing by default
  end

  def output(str)
    raise("Not implemented")
  end

  def error(str)
    raise("Not implemented")
  end

  def to_s()
    raise("Not implemented")
  end

  def attach()
    restore_history()
  end

  def detach()
    save_history()
  end

  def active?()
    raise("Not implemented")
  end

  def attached?()
    raise("Not implemented")
  end

  def go()
    raise("Not implemented")
  end
end
