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

require 'socket'

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

  def initialize(real_cls)
    @name     = real_cls.name()
    @real_cls = real_cls
    Borrower._suppress_warnings do
      eval("::%s = BorrowerStub" % [Borrower.sani(@name)])
    end
  end

  def Borrower.detour_cls(real_cls, fake_cls)
    name = real_cls.name()

    Borrower._suppress_warnings do
      eval("::%s = %s" % [Borrower.sani(name), Borrower.sani(fake_cls.name)])
    end

    yield

    Borrower._suppress_warnings do
      eval("::%s = real_cls" % [Borrower.sani(name)])
    end
  end

  def with_cls(fake_cls)
    Borrower._suppress_warnings do
      eval("::%s = %s" % [Borrower.sani(@name), Borrower.sani(fake_cls.name)])
    end

    yield

    Borrower._suppress_warnings do
      eval("::%s = BorrowerStub" % [Borrower.sani(@name)])
    end
  end

  def Borrower.detour_obj(real_cls, fake_obj)
    name = real_cls.name()
    _ = BorrowerObjectStub.new(fake_obj)

    Borrower._suppress_warnings do
      eval("::%s = _" % [Borrower.sani(name)])
    end

    yield

    Borrower._suppress_warnings do
      eval("::%s = real_cls" % [Borrower.sani(name)])
    end
  end

  def with_obj(fake_obj)
    # This variable looks unused, so we get rid of the warning by making it
    # just an underscore. :)
    _ = BorrowerObjectStub.new(fake_obj)

    Borrower._suppress_warnings do
      eval("::%s = _" % [Borrower.sani(@name)])
    end

    yield

    Borrower._suppress_warnings do
      eval("::%s = BorrowerStub" % [Borrower.sani(@name)])
    end
  end

  def with_real()
    Borrower._suppress_warnings do
      eval("::%s = @real_cls" % [Borrower.sani(@name)])
    end

    yield

    Borrower._suppress_warnings do
      eval("::%s = BorrowerStub" % [Borrower.sani(@name)])
    end
  end
end

class Test
  def initialize()
    puts("initialized!")
  end
  def go()
    puts("gogogo!")
  end
end
