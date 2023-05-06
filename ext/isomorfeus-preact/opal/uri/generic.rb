module URI
  class Generic
    attr_reader :scheme, :userinfo, :user, :password, :host, :port, :path,
      :query, :fragment, :opaque, :registry

    #
    # An Array of the available components for URI::Generic
    #
    COMPONENT = [
      :scheme,
      :userinfo, :host, :port, :registry,
      :path, :opaque,
      :query,
      :fragment
    ]

    def initialize(scheme,
                   userinfo, host, port, registry,
                   path, opaque,
                   query,
                   fragment,
                   parser = nil,
                   arg_check = false)
      @scheme = scheme
      @userinfo = userinfo
      @host = host
      @port = port
      @path = path
      @query = query
      @fragment = fragment
      @user, @password = userinfo.split(/:/) if userinfo
    end

    def ==(other)
      self.class == other.class &&
        component_ary == other.component_ary
    end

    protected

    def component_ary
      self.class::COMPONENT.collect do |x|
        self.send(x)
      end
    end
  end
end
