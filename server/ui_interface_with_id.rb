##
# ui_interface.rb
# By Ron Bowes
# Created May, 2014
#
# See LICENSE.txt
##

class UiInterfaceWithId < UiInterface
  attr_accessor :parent

  def initialize(id)
    NuLog.logging(id) do |msg|
      output(msg)
    end

    super()
  end
end
