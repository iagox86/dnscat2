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

class Borrower
  class BorrowerStub
    def initialize(*args)
      method_missing(*args)
    end

    def method_missing(*args)
      raise(NotImplementedError, "This object has been 'borrowed', but 'I Aten't Dead'! Use the #with_real() method to use this. :)")
    end
  end

  def Borrower._suppress_warnings
    original_verbosity = $VERBOSE
    $VERBOSE = nil
    result = yield
    $VERBOSE = original_verbosity
    return result
  end

  def sani(str)
    return str.gsub(/[^a-zA-Z0-9]/, '')
  end

  def initialize(real_cls)
    @name     = real_cls.name()
    @real_cls = real_cls
    Borrower._suppress_warnings do
      eval("::%s = BorrowerStub" % [sani(@name)])
    end
  end

  def with(fake_obj)
    Borrower._suppress_warnings do
      eval("::%s = %s" % [sani(@name), sani(fake_obj.name)])
    end

    yield

    Borrower._suppress_warnings do
      eval("::%s = BorrowerStub" % [sani(@name)])
    end
  end

  def with_real()
    Borrower._suppress_warnings do
      eval("::%s = @real_cls" % [sani(@name)])
    end

    yield

    Borrower._suppress_warnings do
      eval("::%s = BorrowerStub" % [sani(@name)])
    end
  end
end
