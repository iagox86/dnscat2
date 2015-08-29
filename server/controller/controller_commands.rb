##
# controller_commands.rb
# Created August 29, 2015
# By Ron Bowes
#
# See: LICENSE.md
##

module ControllerCommands
  def _register_commands()
    @commander.register_command('echo',
      Trollop::Parser.new do
        banner("Print stuff to the terminal")
      end,

      Proc.new do |opts, optval|
        @window.puts(optval)
      end
    )

    @commander.register_command('sessions',
      Trollop::Parser.new do
        banner("Lists the current active sessions")
        opt :all, "Show dead sessions", :type => :boolean, :required => false
      end,

      # TODO: We can do much prettier output for 'sessions'
      Proc.new do |opts, optval|
        @sessions.keys.sort.each do |id|
          session = @sessions[id]

          if(opts[:all] || session.state == Session::STATE_ESTABLISHED)
            @window.puts("Session %5d: %s" % [id, session.name()])
          end
        end
      end,
    )
  end
end
