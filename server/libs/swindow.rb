##
# swindow.rb
# By Ron Bowes
# September, 2015
#
# See LICENSE.md
#

# This implements a fairly simple multi-window buffer.
#
# When included, a thread is created that will listen to stdin and feed the
# input to whichever window is active.
#
# New instances of this class are created to create new windows. The window can
# be switched by calling the activate() or deactivate() functions.
#
# Windows are set up like a tree - when you create a window, you can specify a
# 'parent'. When a window is deactivated or closed, the parent is activated (if
# possible).  Typically, you'll want one "master" window, which is the top-most
# window in the tree. 
#
# User input is handled by a callback function. The proc that handles user
# input is passed to the on_input() function (which allows it to be changed),
# and it's called each time the user presses <enter>.
#
# A couple other noteworthy features I haven't mentioned yet:
#
# * Each window's history is tracked up to 1000 lines (by default).. this can
#   be changed by changing SWindow.history_size()
# * Closed windows are still maintained, so users can view them
# * Windows can be enumerated through their parents
# * The puts(), print(), etc functions do exactly what you'd expect, but
#   instead of printing to stdout, they print to the window
# * The puts_ex() function also prints to the window, but it has the option of
#   printing to parent/ancestor/child/descendant windows as well
# * Windows are assigned an incremental id value and can be referred to as such
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

  # This function will trap the TSTP signal (suspend, ctrl-z) and, if possible,
  # activate the parent window.
  def SWindow._catch_suspend()
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

            # If readline() returns nil, it means the input stream is closed
            # (either the file it's reading from is done, or the user pressed
            # ctrl-d). Terminate the input thread.
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
            # If something sent an exit request, we want to break, which shuts
            # down the thread
            break
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

  # Create a new window, with the given parent (use 'nil' for a top-level
  # window, though you should try to only do one of those). Optionally, the
  # window can also be activated (which means it's brought to the front).
  def initialize(parent = nil, activate = false, params = {})
    @parent = parent
    @children = []

    @id = params[:id] || (@@id += 1)
    @name = params[:name] || "unnamed"
    @prompt = params[:prompt] || ("%s %s> " % [@name, @id.to_s()])
    @noinput = params[:noinput] || false

    @callback = nil
    @history = RingBuffer.new(@@history_size)
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

  # Yields for each child
  def children()
    @children.each do |child|
      yield child
    end
  end

  # Set the on_input callback - the function that will be called when input is
  # received. Very important!
  def on_input()
    @callback = proc
  end

  # Write to a window, just like $stdout.puts()
  def puts(str = "")
    _we_just_got_data()

    if(@@active == self)
      $stdout.puts(str)
    end
    @history << (str.to_s() + "\n")
  end

  # Write to a window, just like $stdout.print()
  def print(str = "")
    _we_just_got_data()

    str = str.to_s()
    if(@@active == self)
      $stdout.print(str)
    end
    @history << str.to_s()
  end

  # Write to a window, just like $stdout.printf()
  def printf(*args)
    print(sprintf(*args))
  end

  def _add_child(child)
    @children << child
  end

  # Write to a window, jsut like $stdout.puts(), except that the output can
  # also be printed to parents, ancestors, children, or descendants.
  #
  # Where the input is printed is controlled by the last argument, and the
  # possible values for the last argument are:
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

  # Enable a window; re-draws the history, and starts sending user input to
  # the specified window (note that this can be a closed window; we don't
  # really care)
  def activate()
    # The user just viewed the window, so data is no longer pending
    @pending = false

    # Set this window to the activate one
    @@active = self

    # Re-draw the history
    $stdout.puts(@history.join(""))

    # Fill Readline's buffer with the typed history (this is a bit of a hack,
    # but Readline doesn't support multiple history buffers)
    Readline::HISTORY.clear()
    @typed_history.each do |i|
      Readline::HISTORY << i
    end
  end

  # Basically, this activates the parent window (if possible)
  def deactivate()
    if(@parent)
      @parent.activate()
    else
      $stdout.puts("Can't close the main window!")
    end
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

  # Set the number of lines of history for the current session. Note that this
  # only takes effect after another message is added to the history (lazy
  # evaluated, essentially).
  def history_size=(size)
    @history.max_size = size
  end

  # Get the number of lines of history for the current session.
  def history_size()
    return @history.max_size
  end

  # Set the default history size for new windows that are created. The history
  # size for current windows doesn't change.
  def SWindow.history_size=(size)
    @@history_size = size
  end

  # Get the default history size.
  def SWindow.history_size()
    return @@history_size
  end

  # close the window - closing windows is purely a UI thing, they are still
  # available and can receive data like anything else.
  def close()
    @closed = true
    deactivate()
  end

  # Check if the window has been closed
  def closed?()
    return @closed
  end

  # Check if the window has any pending data
  def pending?()
    return @pending
  end

  # Check if a window with the given id exists
  def SWindow.exists?(id)
    return !@@windows[id.to_s()].nil?
  end

  # Retrieve a window by its id value
  def SWindow.get(id)
    return @@windows[id.to_s()]
  end

  # This function blocks until SWindow is totally finished (that is, it has
  # received an exit signal or an EOF marker).
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
