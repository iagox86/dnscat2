  #############################################################################
  # Class: Vash (Ruby Volatile Hash)
  #
  # Hash that returns values only for a short time.  This is useful as a cache
  # where I/O is involved.  The primary goal of this object is to reduce I/O
  # access and due to the nature of I/O being slower then memory, you should also
  # see a gain in quicker response times.
  #
  # For example, if Person.first found the first person from the database & cache
  # was an instance of Vash then the following would only contact the database for
  # the first iteration:
  #
  # > cache = Vash.new
  # > 1000.times {cache[:person] ||= Person.first}
  #
  # However if you did the following immediately following that command it would
  # hit the database again:
  #
  # > sleep 61
  # > cache[:person] ||= Person.first
  #
  # The reason is that there is a default Time-To-Live of 60 seconds.  You can
  # also set a custom TTL of 10 seconds like so:
  #
  # > cache[:person, 10] = Person.first
  #
  # The Vash object will forget any answer that is requested after the specified
  # TTL.  It is a good idea to manually clean things up from time to time because
  # it is possible that you'll cache data but never again access it and therefor
  # it will stay in memory after the TTL has expired.  To clean up the Vash object,
  # call the method: cleanup!
  #
  # > sleep 11        # At this point the prior person ttl will be expired
  #                   #  but the person key and value will still exist.
  # > cache           # This will still show the the entire set of keys
  #                   #  regardless of the TTL, the :person will still exist
  # > cache.cleanup!  # All of the TTL's will be inspected and the expired
  #                   #  :person key will be deleted.
  #
  # The cleanup must be manually called because the purpose of the Vash is to
  # lessen needless I/O calls and gain speed not to slow it down with regular
  # maintenance.
  class Vash < Hash
    def initialize(constructor = {})
      @register ||= {} # remembers expiration time of every key
      if constructor.is_a?(Hash)
        super()
        merge(constructor)
      else
        super(constructor)
      end
    end

    alias_method :regular_writer, :[]= unless method_defined?(:regular_writer)
    alias_method :regular_reader, :[] unless method_defined?(:regular_reader)

    def [](key)
      sterilize(key)
      clear(key) if expired?(key)
      regular_reader(key)
    end

    def []=(key, *args)
      # a little bit o variable hacking to support (h[key, ttl] = value), which will come
      # accross as (key, [ttl, value]) whereas (h[key]=value) comes accross as (key, [value])
      if args.length == 2
        value, ttl = args[1], args[0]
      elsif args.length == 1
        value, ttl = args[0], 10
      else
        raise ArgumentError, "Wrong number of arguments, expected 2 or 3, received: #{args.length+1}\n"+
                             "Example Usage:  volatile_hash[:key]=value OR volatile_hash[:key, ttl]=value"
      end
      sterilize(key)
      ttl(key, ttl)
      regular_writer(key, value)
    end

    def merge(hsh)
      hsh.map {|key,value| self[sterile(key)] = hsh[key]}
      self
    end

    def cleanup!
      now = Time.now.to_i
      @register.map {|k,v| clear(k) if v < now}
    end

    def clear(key)
      sterilize(key)
      @register.delete key
      self.delete key
    end

  private
    def expired?(key)
      Time.now.to_i > @register[key].to_i
    end

    def ttl(key, secs=60)
      @register[key] = Time.now.to_i + secs.to_i
    end

    def sterile(key)
      String === key ? key.chomp('!').chomp('=') : key.to_s.chomp('!').chomp('=').to_sym
    end

    def sterilize(key)
      key = sterile(key)
    end
  end
