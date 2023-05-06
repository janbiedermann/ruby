module Isomorfeus
  class ThreadLocalComponentCache
    def initialize
      Thread.current[:local_cache] = {} unless Thread.current.key?(:local_cache)
    end

    def fetch(key)
      Thread.current[:local_cache][key]
    end

    def store(key, rendered_tree, styles, status)
      Thread.current[:local_cache][key] = [rendered_tree, styles, status]
    end
  end
end
