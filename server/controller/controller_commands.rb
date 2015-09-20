##
# controller_commands.rb
# Created August 29, 2015
# By Ron Bowes
#
# See: LICENSE.md
##

require 'tunnel_drivers/tunnel_drivers'

module ControllerCommands

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
        CommandHelpers.display_windows(@window, opts[:all], @window)
      end,
    )

    @commander.register_command("window",
      Trollop::Parser.new do
        banner("Interact with a window")
        opt :i, "Interact with the chosen window", :type => :string, :required => false
      end,

      Proc.new do |opts, optarg|
        if(opts[:i].nil?)
          CommandHelpers.display_windows(@window, opts[:all], @window)
          next
        end

        window = SWindow.get(opts[:i])
        if(window.nil?)
          @window.puts("Windown #{opts[:i]} not found!")
          @window.puts()
          CommandHelpers.display_windows(@window, false, @window)
          next
        end

        window.activate()
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
          @window.puts("** Global options:")
          @window.puts()
          Settings::GLOBAL.each_setting() do |name, value, docs, default|
            @window.puts("%s => %s [default = %s]" % [name, CommandHelpers.format_field(value), CommandHelpers.format_field(default)])
            @window.puts(CommandHelpers.wrap(docs, 72, 4))
            @window.puts()
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

    @commander.register_command("unset",
      Trollop::Parser.new do
        banner("unset <name>")
      end,

      Proc.new do |opts, optarg|
        Settings::GLOBAL.unset(optarg)
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
        banner("Kill the specified session or tunnel driver")
      end,

      Proc.new do |opts, optarg|
        if(TunnelDrivers.exists?(optarg))
          @window.puts("Attempting to stop the tunnel driver: #{optarg}")
          TunnelDrivers.stop(optarg)
          next
        end

        session = find_session_by_window(optarg)
        if(!session)
          @window.puts("Couldn't find window with id = #{optarg}")
          next
        end

        @window.puts("Session #{optarg} has been sent the kill signal!")
        session.kill()
      end
    )

    @commander.register_command("start",
      Trollop::Parser.new do
        banner("Start a new tunnel_driver - currently, this just means another\n" +
               "DNS driver. The 'startdns' command can also be used for simpler\n" +
               "syntax.\n" +
               "\n" +
               "The protocol (--dns) must be specified, and all information\n" +
               "about the DNS server should be passed as name=value pairs, where\n" +
               "the following names are possible:\n" +
               "\n" +
               "domain=<domain>       The domain to listen for requests on\n" +
               "                      (optional)\n" +
               "host=<hostname>       The host to listen on (default: 0.0.0.0).\n" +
               "port=<port>           The port to listen on (default: 53).\n" +
               "\n" +
               " Examples:\n" +
               "  start --dns domain=skullseclabs.org\n" +
               "  start --dns domain=skullseclabs.org,port=53\n" +
               "  start --dns domain=skullseclabs.org,port=5353\n" +
               "\n" +
               "To stop a driver, simply use the 'kill' command on the window\n" +
               "it created (td1, td2, etc)\n" +
               "\n")

        opt :dns, "Start a DNS instance", :type => :string, :required => false
      end,

      Proc.new do |opts, optarg|
        if(opts[:dns].nil?)
          @window.puts("No --dns argument was passed!")
          @window.puts()
          raise(Trollop::HelpNeeded)
        end

        args = {
          :domain => nil,
          :host => "0.0.0.0",
          :port => "53",
        }

        opts[:dns].split(/[,;]/).each do |arg|
          name, value = arg.split(/=/, 2)
          name = name.strip().to_sym()
          value = value.strip()

          if(value.nil?)
            @window.puts("The --dns argument requires name=value pairs,")
            @window.puts("separated by commas. See examples below.")
            @window.puts()
            raise(Trollop::HelpNeeded)
          end

          if(!args.has_key?(name))
            @window.puts("The --dns argument requires certain name=value pairs,")
            @window.puts("separated by commas, but this one isn't valid:")
            @window.puts("#{name}")
            @window.puts()
            raise(Trollop::HelpNeeded)
          end

          args[name] = value
        end

        if(!args[:domain].nil?)
          args[:domain] = [args[:domain]]
        end

        TunnelDrivers.start(self, DriverDNS.new(args[:host], args[:port], args[:domain], @window))
      end
    )
  end
end
