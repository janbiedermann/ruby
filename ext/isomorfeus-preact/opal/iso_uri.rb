require "uri/common"
require "uri/generic"

module URI
  @@schemes = {}

  class InvalidURIError < Exception
  end
  
  def self.parse(url)
      # scheme://conn_data/path?query#hash
      match = url.match(%r[(\w+)://([^/]+)([^\?]+)(\?[^\#]+)?(\#.*)?])

      _, scheme, connection_data, path, query, fragment = match.to_a

      connection_data = connection_data.split(/@/)
      userinfo = connection_data.size > 1 ? connection_data.first : nil
      host, port = connection_data.last.split(/:/)

      port = port.to_i
      query = query[1..-1] if query

      registry = opaque = nil

      Generic.new(scheme, userinfo, host, port,
                  registry, path, opaque, query,
                  fragment)
  end
end
