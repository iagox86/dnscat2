##
# driver_console.rb
# Created September 16, 2015
# By Ron Bowes
#
# See: LICENSE.md
#
##

require 'open3'

class DriverProcess
  def initialize(window, settings, process)
    @window = window
    @settings = settings
    @outgoing = ""
    @window.noinput = true

    @window.puts("This isn't a console session!")
    @window.puts()
    @window.puts("The 'process' variable is set, which means that a specific")
    @window.puts("process:")
    @window.puts()
    @window.puts(process)
    @window.puts()
    @window.puts("will be started. That process's i/o is bound to that dnscat2")
    @window.puts("client, which means you can interact with the process via")
    @window.puts("that client.")
    @window.puts("")
    @window.puts("Note that there is no access control, which means any client")
    @window.puts("that connects to this server can also use this process; there")
    @window.puts("are some security implications there!")
    @window.puts()
    @window.puts("To disable this, run 'set process=' in the main window.")
    @window.puts()
    @window.puts("To go back, type ctrl-z.")
    @window.puts()

    @done = false

    # Do this in a thread, since read() blocks
    @thread = Thread.new() do |thread|
      # Put this in an error block, since Threads don't print errors when debug is off
      begin
        # popen2e combines stderr and stdout into a single pipe, which is handy
        @window.puts("Starting process: #{process}")
        Open3.popen2e(process) do |stdin, stdout, wait_thr|
          # Save stdin so we can write to it when data comes
          @process_stdin = stdin

          # Read the output character by character.. I'm not sure if there's a better way, .gets() isn't
          # binary friendly, and reading more than 1 byte means that buffering happens
          while line = stdout.read(1)
            @outgoing += line
          end

          # Get the exit status
          exit_status = wait_thr.value

          if(!exit_status.success?)
            @window.puts("Command exited with an error: #{process}")
          else
            @window.puts("Command exited successfully: #{process}")
          end

          @done = true

        end
      rescue Exception => e
        $stdout.puts("ERROR: #{e}")
      end
    end
  end

  def feed(data)
    if(@done)
      return nil
    end

    @window.puts("[-->] #{data}")
    @process_stdin.write(data)

    out = @outgoing
    @outgoing = ''

    @window.puts("[<--] #{out}")
    return out
  end
end
