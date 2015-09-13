require 'readline'

class SWindow
  attr_accessor :prompt, :name
  attr_reader :id

  @@id = -1
  @@active = nil
  @@windows = {}

  def SWindow._catch_suspend()
    # Trap ctrl-z, just like Metasploit
    orig_suspend = Signal.trap("TSTP") do
      if(@@active)
        @@active.deactivate()
      end
    end

    proc.call()

    Signal.trap("TSTP", orig_suspend)
  end

  @@input_thread = Thread.new() do
    begin
      _catch_suspend() do
        loop do
          begin
            while @@active.nil? do
            end

            str = Readline::readline(@@active.prompt, true)

            if(str.nil?)
              break
            end
            if(@@active.nil?)
              $stderr.puts("WARNING: there is no active session! Input's going nowhere")
              next
            end

            @@active._incoming(str)
          end
        end
      end

      $stderr.puts("Input thread is over")
    rescue Exception => e
      $stderr.puts(e)
      $stderr.puts(e.backtrace.join("\n"))
    end
  end

  def initialize(parent = nil, activate = false, params = {})
    @parent = parent
    @children = []

    @id = params[:id] || (@@id += 1)
    @name = params[:name] || "unnamed"
    @prompt = params[:prompt] || ("%s %d> " % [@name, @id])

    @callback = nil
    @history = []
    @typed_history = []
    @closed = false

    if(@parent)
      @parent._add_child(self)
    end

    if(@@active.nil? || activate)
      self.activate(false)
    end

    if(params[:quiet] != true)
      self.puts_ex("New session created: %s" % @id.to_s(), true, true, false, false)
    end

    @@windows[@id.to_s()] = self
  end

  def children()
    @children.each do |child|
      yield child
    end
  end

  def on_input()
    @callback = proc
  end

  def puts(str = "")
    str = str.to_s()
    if(@@active == self)
      $stdout.puts(str)
    end
    @history << (str + "\n")
  end

  def print(str = "")
    str = str.to_s()
    if(@@active == self)
      $stdout.print(str)
    end
    @history << str
  end

  def _add_child(child)
    @children << child
  end

  def puts_ex(str, to_parent = false, to_grandparents = false, to_children = false, to_grandchildren = false)
    puts(str)

    if(to_grandparents)
      @parent.puts_ex(str, false, true, false, false) if(@parent)
    elsif(to_parent)
      @parent.puts_ex(str, false, false, false, false) if(@parent)
    end

    if(to_grandchildren)
      @children.each do |child|
        child.puts_ex(str, false, false, false, true)
      end
    elsif(to_children)
      @children.each do |child|
        child.puts_ex(str, false, false, false, false)
      end
    end
  end

  def _redraw()
    # TODO: Have a flag that can turn this off
    $stdout.puts(@history.join(""))
    $stdout.puts(@prompt)
  end

  def activate(redraw = true)
    @@active = self
    if(redraw)
      self._redraw()
    end

    Readline::HISTORY.clear()
    @typed_history.each do |i|
      Readline::HISTORY << i
    end
  end

  def SWindow.activate(id, redraw = true)
    puts("Param: '#{id}' #{id.class}")
    window = @@windows[id.to_s()]
    if(window.nil?)
      return false
    end

    window.activate(redraw)
    return true
  end

  def deactivate()
    if(@parent)
      @parent.activate()
    else
      $stdout.puts("Can't close the main window!")
    end
  end

  def SWindow.deactivate(id)
    window = @@windows[id.to_s()]
    if(window.nil?)
      return false
    end

    window.deactivate()
    return true
  end

  def _incoming(str)
    @history << @prompt + str + "\n"
    @typed_history << str

    if(@callback.nil?)
      self.puts("Input received, but nothing has registered to receive it")
      self.puts("Please wait and try again in a bit!")
      return
    end
    @callback.call(str)
  end

  def closed?()
    return @closed
  end

  def close()
    @closed = true
    deactivate()
  end

  def SWindow.close(id)
    window = @@windows[id.to_s()]
    if(window.nil?)
      return false
    end

    window.close()
    return true
  end

  def SWindow.wait()
    @@input_thread.join()
  end

  def to_s()
    return "%s :: %s" % [@id.to_s(), @name]
  end
end
