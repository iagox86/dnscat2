# This class should be totally stateless, and rely on the Session class
# for any long-term session storage
class Log
  def Log.log(title, message)
    puts("[[#{title}]] :: #{message}")
  end
end

