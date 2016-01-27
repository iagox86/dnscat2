##
# borrower.rb
# By Ron Bowes
# Created January 25, 2016
#
# See LICENSE.md
#
# I wrote this class to solve a weird problem: I needed to "take over" the
# TCPSocket class once in awhile, to redirect the traffic across a tunnel
# instead of going straight to the Internet.
##

#require 'socket'

class Borrower
  class BorrowerStub
    def initialize(*args)
      method_missing(*args)
    end

    def method_missing(*args)
      raise(NotImplementedError, "This object has been 'borrowed', but 'I Aten't Dead'! Use the #with_real() method to use this. :)")
    end
  end

  class BorrowerObjectStub
    def initialize(obj)
      @obj = obj
    end

    def new()
      return @obj
    end
  end

  def Borrower._suppress_warnings
    original_verbosity = $VERBOSE
    $VERBOSE = nil
    result = yield
    $VERBOSE = original_verbosity
    return result
  end

  def Borrower.sani(str)
    return str.gsub(/[^a-zA-Z0-9]/, '')
  end

  def Borrower.detour(real, fake, params = {})
    is_obj = params[:is_obj]

    name = real.name()

    Borrower._suppress_warnings do
      if(is_obj)
        stub = BorrowerObjectStub.new(fake)
        # This 'if' is only to get rid of an unused variable warning
        if(stub)
          fake = "stub"
        end
      else
        stub = fake
        fake = "stub"
      end

      eval("::%s = %s" % [Borrower.sani(name), Borrower.sani(fake)])
    end

    yield

    Borrower._suppress_warnings do
      eval("::%s = real" % [Borrower.sani(name)])
    end
  end
end

#class Test
#  def initialize(a="nothing")
#    puts("initialized! #{a}")
#  end
#  def go()
#    puts("gogogo!")
#  end
#end
#
#class TestStub
#  def initialize(important)
#    @important = important
#  end
#
#  def new()
#    return Test.new(@important)
#  end
#
#  def name()
#    return "TestStub"
#  end
#end
#
#puts(TCPSocket)
#Borrower.detour(TCPSocket, TestStub.new('IMPORTANT!')) do
#  puts(TCPSocket)
#  socket = TCPSocket.new()
#  puts(socket)
#end
#puts(TCPSocket)
