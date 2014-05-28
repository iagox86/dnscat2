# session_handler.rb
# By Ron Bowes
# Created May, 2014

module UiHandler
  def initialize_ui_handler()
    @pending = {}
    @uis = []
  end

  def add_pending(real_id)
    @pending[real_id] = true
  end

  # Callback
  def ui_created(ui, local_id, real_id)
    if(@pending[real_id])
      @pending.delete(real_id)
      @uis << ui
      output("Child session established: %d" % local_id)
    end
  end

  def pending_count()
    return @pending.length
  end

  def each_child_ui()
    @uis.each do |ui|
      yield(ui)
    end
  end

  def display_uis(all, indent = "")
    puts(self.to_s)
    indent += " "
    each_child_ui() do |ui|
      if(all || ui.active?)
        puts(indent + ui.to_s)
        if(ui.respond_to?(:display_uis))
          ui.display_uis(indent + " ")
        end
      end

      if(all && pending_count() > 0)
        puts()
        puts("We also have %d pending sessions" % pending_count())
      end
    end
  end
end
