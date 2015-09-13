##
# controller_commands.rb
# Created August 29, 2015
# By Ron Bowes
#
# See: LICENSE.md
##

module ControllerCommands
  def _display_window(window, all, indent = 0)
    if(!all && !window.pending?() && window.closed?())
      return
    end

    @window.puts(('  ' * indent) + window.to_s())
    window.children() do |c|
      _display_window(c, all, indent + 1)
    end
  end

  def _display_windows(all)
    _display_window(@window, all)
  end

  def _register_commands()
    @commander.register_alias('sessions', 'windows')
    @commander.register_alias('session',  'window')
    @commander.register_alias('q',        'quit')
    @commander.register_alias('exit',     'quit')
    @commander.register_alias('h',        'help')
    @commander.register_alias('?',        'help')

    @commander.register_command('help',
      Trollop::Parser.new do
        banner("Shows a help menu")
      end,

      Proc.new do |opts, optval|
        @commander.help(@window)
      end,
    )

    @commander.register_command('echo',
      Trollop::Parser.new do
        banner("Print stuff to the terminal")
      end,

      Proc.new do |opts, optarg|
        @window.puts(optarg)
      end
    )

    @commander.register_command('windows',
      Trollop::Parser.new do
        banner("Lists the current active windows")
        opt :all, "Show closed windows", :type => :boolean, :required => false
      end,

      Proc.new do |opts, optarg|
        _display_windows(opts[:all])
      end,
    )

    @commander.register_command("window",
      Trollop::Parser.new do
        banner("Interact with a window")
        opt :i, "Interact with the chosen window", :type => :string, :required => false
      end,

      Proc.new do |opts, optarg|
        if(opts[:i].nil?)
          _display_windows(opts[:all])
          next
        end

        if(!SWindow.activate(opts[:i]))
          @window.puts("Windown #{opts[:i]} not found!")
          @window.puts()
          _display_windows(false)
          next
        end
      end
    )

    @commander.register_command("set",
      Trollop::Parser.new do
        banner("set <name>=<value>")
      end,

      Proc.new do |opts, optarg|
        if(optarg.length == 0)
          @window.puts("Usage: set <name>=<value>")
          @window.puts()
          @window.puts("Global options:")
          Settings::GLOBAL.each_pair() do |k, v|
            @window.puts(" %s=%s" % [k, v.to_s()])
          end

          next
        end

        # Split at the '=' sign
        namevalue = optarg.split("=", 2)

        if(namevalue.length != 2)
          namevalue = optarg.split(" ", 2)
        end

        if(namevalue.length != 2)
          @window.puts("Bad argument! Expected: 'set <name>=<value>' or 'set name value'!")
          @window.puts()
          raise(Trollop::HelpNeeded)
        end

        begin
          Settings::GLOBAL.set(namevalue[0], namevalue[1], false)
        rescue Settings::ValidationError => e
          @window.puts("Failed to set the new value: #{e}")
        end
      end
    )

    @commander.register_command("quit",
      Trollop::Parser.new do
        banner("Close all sessions and exit dnscat2")
      end,

      Proc.new do |opts, optarg|
        exit(0)
      end
    )

    @commander.register_command("kill",
      Trollop::Parser.new do
        banner("Kill the specified session")
      end,

      Proc.new do |opts, optarg|
        session = find_session_by_window(optarg)
        if(!session)
          @window.puts("Couldn't find window with id = #{optarg}")
          next
        end

        session.kill()
        @window.puts("Session #{optarg} has been sent the kill signal!")
      end
    )

  end
end
