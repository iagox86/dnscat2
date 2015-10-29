##
# dnscat_exception.rb
# Created July 1, 2013 (Canada Day!)
# By Ron Bowes
#
# See LICENSE.md
#
# Implements a simple exception class for dnscat2 protocol errors.
##

class DnscatException < StandardError
end

# Use this for exceptions that aren't as major
class DnscatMinorException < DnscatException
end
