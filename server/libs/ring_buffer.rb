##
# ring_buffer.rb
# By https://gist.github.com/Nimster/4078106
# Created Sept 18, 2015
#
# See LICENSE.md
##

class RingBuffer < Array
  attr_accessor :max_size

  def initialize(max_size, enum = nil)
    @max_size = max_size
    enum.each { |e| self << e } if enum
  end

  def <<(el)
    if self.size < @max_size || @max_size.nil?
      super
    else
      self.shift
      self.push(el)
    end
  end

  alias :push :<<
end


