##
# subscriber.rb
# Created April 23, 2014
# By Ron Bowes
#
# See: LICENSE.txt
##

module Subscribable
  # Begin subscriber stuff (this should be in a mixin, but static stuff doesn't
  # really seem to work
  def subscribe(cls)
    @subscribers = @subscribers || []
    @subscribers << cls
  end

  def unsubscribe(cls)
    @subscribers = @subscribers || []
    @subscribers.delete(cls)
  end

  def notify_subscribers(method, args)
    @subscribers = @subscribers || []

    @subscribers.each do |subscriber|
      if(subscriber.respond_to?(method))
        subscriber.method(method).call(*args)
      end
    end
  end

  def get_subscribers()
    @subscribers = @subscribers || []
    return @subscribers
  end

  def bestow_subscribers_upon(cls)
    @subscribers = @subscribers || []
    @subscribers.each do |s|
      cls.subscribe(s)
    end
  end
end

#class MySubscriber
#  def method1()
#    puts("Method 1!")
#  end
#  def method2()
#    puts("Method 2!")
#  end
#  def initialize()
#    puts("Creating MySubscriber")
#  end
#end
#
#class Test1
#  include Subscribable
#
#  def initialize()
#
#  end
#end
#
#class Test2
#  include Subscribable
#
#  def initialize()
#
#  end
#end
#
#a = Test1.new()
#s = MySubscriber.new()
#
## Both methods should fire
#a.subscribe(s)
#a.notify_subscribers(:method1, [])
#a.notify_subscribers(:method2, [])
#
## Nothing should be displayed
#b = Test1.new()
#b.notify_subscribers(:method1, [])
#b.notify_subscribers(:method2, [])
#
## Nothing should be displayed
#c = Test2.new()
#c.notify_subscribers(:method1, [])
#c.notify_subscribers(:method2, [])
#
## Both methods should fire
#c.subscribe(MySubscriber.new())
#c.notify_subscribers(:method1, [])
#c.notify_subscribers(:method2, [])
