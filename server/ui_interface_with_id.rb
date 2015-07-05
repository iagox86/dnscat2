##
# ui_interface_with_id.rb
# By Ron Bowes
# Created May, 2014
#
# See LICENSE.md
##

require 'log'

class UiInterfaceWithId < UiInterface
  attr_accessor :parent, :id

  def initialize(id)
    @id = id
    Log.logging(id) do |msg|
      output(msg)
    end

    super()
  end
end
