##
# log.rb
# Created March, 2013
# By Ron Bowes
#
# See: LICENSE.txt
#
# A very simple logging class. May improve this later, but for now it can be
# as simple as necessary.
##
class Log
  def Log.log(title, message)
    puts("[[#{title}]] :: #{message}")
  end
end

