require 'readline'

class SWindow
  attr_reader :prompt
  @@id = 0
  @@active = nil

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

  @@id = 0

  def initialize(name = nil, prompt = nil, parent = nil, activate = false)
    @id = (@@id += 1)
    @name = name || "unnamed"
    @prompt = prompt || "%s %d>" % [@name, @id]
    @parent = parent
    @children = []
    @callback = proc
    @history = [""] * 100
    @typed_history = []

    if(@parent)
      @parent._add_child(self)
    end

    puts("New session created: #{@name}")
    if(@@active.nil? || activate)
      self.activate(false)
    end
  end

  def spawn(name = nil, prompt = nil)
    return SWindow.new(name, prompt, self, &proc)
  end

  def puts(str)
    if(@@active == self)
      $stdout.puts(str)
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

  def redraw()
    $stdout.puts("Hi?")
    $stdout.puts(@history.join("\n"))
    $stdout.puts(@prompt)
  end

  def activate(redraw = true)
    @@active = self
    if(redraw)
      self.redraw()
    end

    Readline::HISTORY.clear()
    @typed_history.each do |i|
      Readline::HISTORY << i
    end
  end

  def deactivate()
    if(@parent)
      @parent.activate()
    else
      $stdout.puts("No parent to activate")
    end
  end

  def _incoming(str)
    @history << str
    @typed_history << str
    @callback.call(str)
  end
end

def myfunc(str)

end


windows = {}

windows['A'] = SWindow.new("A") do |str|
  windows['A'].puts("A: #{str}")
  windows[str].activate if windows[str]
end

windows['child1'] = windows['A'].spawn("child1") do |str|
  windows['child1'].puts("child1: #{str}")
  windows[str].activate if windows[str]
end
windows['child2'] = windows['A'].spawn("child2") do |str|
  windows['child2'].puts("child2: #{str}")
  windows[str].activate if windows[str]
end
windows['child3'] = windows['A'].spawn("child3") do |str|
  windows['child3'].puts("child3: #{str}")
  windows[str].activate if windows[str]
end

windows['child1_child1'] = windows['child1'].spawn("child1_child1") do |str|
  windows['child1_child1'].puts("child1_child1: #{str}")
  windows[str].activate if windows[str]
end
windows['child1_child2'] = windows['child1'].spawn("child1_child2") do |str|
  windows['child1_child2'].puts("child1_child2: #{str}")
  windows[str].activate if windows[str]
end

windows['child3_child1'] = windows['child3'].spawn("child3_child1") do |str|
  windows['child3_child1'].puts("child3_child1: #{str}")
  windows[str].activate if windows[str]
end

windows['child1_child1_child1'] = windows['child1_child1'].spawn("child1_child1_child1") do |str|
  windows['child1_child1_child1'].puts("child1_child1_child1: #{str}")
  windows[str].activate if windows[str]
end

windows['child1_child1_child1_child1'] = windows['child1_child1_child1'].spawn("child1_child1_child1_child1") do |str|
  windows['child1_child1_child1_child1'].puts("child1_child1_child1_child1: #{str}")
  windows[str].activate if windows[str]
end

windows['B'] = SWindow.new("B") do |str|
  windows['B'].puts("Received something from B: #{str}")
  windows[str].activate if windows[str]
end

windows['A'].puts_ex("This should just go to 'A'")
windows['A'].puts_ex("This should go to A and its children", false, false, true, false)
windows['A'].puts_ex("This should go to A and all of its children/grandchildren [1]", false, false, true, true)
windows['A'].puts_ex("This should go to A and all of its children/grandchildren [2]", true,  false, true, true)
windows['A'].puts_ex("This should go to A and all of its children/grandchildren [3]", false, true, true, true)
windows['child1_child1_child1_child1'].puts_ex("This should go to the deepest grandchild and nothing else", false, false, false, false)
windows['child1_child1_child1_child1'].puts_ex("This should go to the deepest grandchild and nothing else", false, false, true, false)
windows['child1_child1_child1_child1'].puts_ex("This should go to the deepest grandchild and nothing else", false, false, true, true)
windows['child1_child1_child1_child1'].puts_ex("This should go to the deepest grandchild and its parent", true, false, false, false)
windows['child1_child1_child1_child1'].puts_ex("This should go to the deepest grandchild and all of its parents/grandparents", true, true, false, false)


#def puts_ex(str, to_parent = false, to_grandparents = false, to_children = false, to_grandchildren = false)
sleep(1000)


