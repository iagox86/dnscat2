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
    super()

    NuLog.logging(id) do |msg|
      output(msg)
    end
  end
end
