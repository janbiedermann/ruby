module Isomorfeus
  if RUBY_ENGINE == 'opal'
    class << self
      attr_accessor :browser_history
      attr_accessor :browser_location
      attr_accessor :current_user_sid_s
      attr_accessor :ssr_response_status
      attr_accessor :top_component
      attr_reader :initialized
      attr_reader :env

      def init
        return if initialized
        @initialized = true
        Isomorfeus.init_store
        execute_init_classes
      end

      def force_init!
        @initialized = true
        Isomorfeus.force_init_store!
        execute_init_classes
      end

      def start_app!
        Isomorfeus.zeitwerk.setup
        Isomorfeus::TopLevel.mount!
      end

      def force_render
        Preact.unmount_component_at_node(`document.body`)
        Isomorfeus::TopLevel.do_the_mount!(init: false)
        nil
      rescue Exception => e
        `console.error("force_render failed'! Error: " + #{e.message} + "! Reloading page.")`
        `location.reload()`
      end
    end
  else # RUBY_ENGINE
    class << self
      attr_reader :component_cache_init_block
      attr_accessor :ssr_hot_asset_url

      def ssr_response_status
        Thread.current[:@_isomorfeus_preact_ssr_response_status]
      end

      def ssr_response_status=(s)
        Thread.current[:@_isomorfeus_preact_ssr_response_status] = s
      end

      def init
        return if Thread.current[:@_isomorfeus_initialized]
        Thread.current[:@_isomorfeus_initialized] = true
        Isomorfeus.init_store
        execute_init_classes
      end

      def force_init!
        Thread.current[:@_isomorfeus_initialized] = true
        Isomorfeus.force_init_store!
        execute_init_classes
      end

      def browser_history
        @_isomorfeus_browser_history
      end

      def browser_history=(h)
        @_isomorfeus_browser_history = h
      end

      def browser_location
        Thread.current[:@_isomorfeus_browser_location]
      end

      def browser_location=(l)
        Thread.current[:@_isomorfeus_browser_location] = l
      end

      def component_cache_init(&block)
        @component_cache_init_block = block
      end

      def configuration(&block)
        block.call(self)
      end

      def ssr_contexts
        @ssr_contexts ||= {}
      end

      def version
        Isomorfeus::VERSION
      end

      def load_configuration(directory)
        Dir.glob(File.join(directory, '*.rb')).sort.each do |file|
          require File.expand_path(file)
        end
      end
    end
  end # RUBY_ENGINE

  class << self
    def raise_error(error: nil, error_class: nil, message: nil, stack: nil)
      error_class = error.class if error

      error_class = RuntimeError unless error_class
      execution_environment = if on_browser? then 'on Browser'
                              elsif on_server? then 'on Server'
                              else
                                'on Client'
                              end
      if error
        message = error.message
        stack = error.backtrace
      else
        error = error_class.new("Isomorfeus in #{env} #{execution_environment}:\n#{message}")
        error.set_backtrace(stack) if stack
      end

      ecn = error_class ? error_class.name : ''
      m = message ? message : ''
      s = stack ? stack : ''
      if RUBY_ENGINE == 'opal'
        `console.error(ecn, m, s)` if Isomorfeus.development?
      else
        STDERR.puts "#{ecn}: #{m}\n #{s.is_a?(Array) ? s.join("\n") : s}"
      end
      raise error
    end
  end
end
