module Browser
  class Location
    if RUBY_ENGINE == 'opal'
      include Native::Wrapper

      alias_native :hash
      alias_native :host
      alias_native :hostname
      alias_native :href
      alias_native :origin
      alias_native :pathname
      alias_native :port
      alias_native :protocol
      alias_native :search
    else
      def initialize(location_string)
        @location = URI(location_string)
      rescue
        @location = URI('http://localhost/')
      end

      def hash
        @location.hash
      end

      def host
        @location.host
      end

      def hostname
        @location.hostname
      end

      def href
        @location.to_s
      end

      def origin
        @location.origin
      end

      def pathname
        @location.path
      end

      def port
        @location.port
      end

      def protocol
        @location.scheme
      end

      def search
        @location.query
      end
    end
  end
end
