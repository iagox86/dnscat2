##
# swindow.rb
# By Ron Bowes
# September, 2015
#
# See LICENSE.md
##

require 'readline'

require 'libs/ring_buffer'

class SWindow
  attr_accessor :prompt, :name, :noinput
  attr_reader :id

  @@id = -1
  @@active = nil
  @@windows = {}
  @@history_size = 1000

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

            if(@@active.noinput)
              str = Readline::readline()
            else
              str = Readline::readline(@@active.prompt, true)
            end

            if(str.nil?)
              break
            end
            if(@@active.nil?)
              $stderr.puts("WARNING: there is no active window! Input's going nowhere")
              $stderr.puts("If you think this might be a bug, please report to")
              $stderr.puts("https://github.com/iagox86/dnscat2/issues")
              next
            end

            @@active._incoming(str)
          rescue SystemExit
            exit(0)
          rescue Exception => e
            $stderr.puts("Something bad just happened! You will likely want to report this to")
            $stderr.puts("https://github.com/iagox86/dnscat2/issues")
            $stderr.puts(e.inspect)
            $stderr.puts(e.backtrace.join("\n"))
          end
        end
      end

      $stderr.puts("Input thread is over")
    rescue StandardError => e
      $stderr.puts(e)
      $stderr.puts(e.backtrace.join("\n"))
    end
  end

  def initialize(parent = nil, activate = false, params = {})
    @parent = parent
    @children = []

    @id = params[:id] || (@@id += 1)
    @name = params[:name] || "unnamed"
    @prompt = params[:prompt] || ("%s %s> " % [@name, @id.to_s()])
    @noinput = params[:noinput] || false

    @callback = nil
    @history = RingBuffer.new(5)
    @typed_history = []
    @closed = false
    @pending = false

    if(@parent)
      @parent._add_child(self)
    end

    if(@@active.nil? || activate)
      self.activate()
    end

    if(params[:quiet] != true)
      self.puts_ex("New window created: %s" % @id.to_s(), {:to_ancestors=>true})
    end

    @@windows[@id.to_s()] = self
  end

  def _we_just_got_data()
    if(@@active == self)
      return
    end

    @pending = true
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
    _we_just_got_data()

    if(@@active == self)
      $stdout.puts(str)
    end
    @history << (str.to_s() + "\n")
  end

  def print(str = "")
    _we_just_got_data()

    str = str.to_s()
    if(@@active == self)
      $stdout.print(str)
    end
    @history << str.to_s()
  end

  def printf(*args)
    print(sprintf(*args))
  end

  def _add_child(child)
    @children << child
  end

  # Possible arguments:
  #  :to_parent
  #  :to_ancestors
  #  :to_children
  #  :to_descendants
  def puts_ex(str, params = {})
    # I keep using these by accident, so just handle them
    params.each_key do |k|
      if(![:to_parent, :to_ancestors, :to_children, :to_descendants].index(k))
        puts("oops: #{k} doesn't exist!")
      end
    end

    puts(str)

    if(params[:to_parent] || params[:to_ancestors])
      parent_params = params.clone()
      parent_params[:to_parent] = false
      parent_params[:to_children] = false
      parent_params[:to_descendants] = false

      if(@parent)
        @parent.puts_ex(str, parent_params)
      end
    end

    if(params[:to_children] || params[:to_descendants])
      child_params = params.clone()
      child_params[:to_child] = false
      child_params[:to_parent] = false
      child_params[:to_ancestors] = false

      @children.each do |child|
        child.puts_ex(str, child_params)
      end
    end
  end

  def activate()
    @pending = false

    @@active = self
    $stdout.puts(@history.join(""))

    Readline::HISTORY.clear()
    @typed_history.each do |i|
      Readline::HISTORY << i
    end
  end

  def SWindow.activate(id)
    window = @@windows[id.to_s()]
    if(window.nil?)
      return false
    end

    window.activate()
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
    if(@noinput)
      return
    end

    @history << @prompt + str + "\n"
    if(str != '')
      @typed_history << str
    end

    if(@callback.nil?)
      self.puts("Input received, but nothing has registered to receive it")
      self.puts("Use ctrl-z to escape if this window isn't taking input!")
      return
    end
    @callback.call(str)
  end

  def history_size=(size)
    @history.max_size = size
  end

  def SWindow.history_size=(size)
    @@history_size = size
  end

  def SWindow.history_size()
    return @@history_Size
  end

  def closed?()
    return @closed
  end

  def pending?()
    return @pending
  end

  def SWindow.exists?(id)
    return !@@windows[id.to_s()].nil?
  end

  def SWindow.get(id)
    return @@windows[id.to_s()]
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
    s = "%s :: %s" % [@id.to_s(), @name]
    if(@@active == self)
      s += " [active]"
    end

    if(@pending)
      s += " [*]"
    end

    return s
  end
end
