module Preact
  class Context
    def self.create_application_contexts
      Preact.create_context('LucidApplicationContext', { iso_location: "/", iso_store: {}, iso_theme: {} })
      Preact.create_context('RouterContext', {})
    end

    class Provider < Preact::Component
      class << self
        attr_reader :root_context
      end

      attr_reader :context_id
      attr_reader :subs

      def initialize(props, context)
        @self_class_root = self.class.root_context
        unless props.key?(:value)
          props = props.merge({ value: @self_class_root.value })
        end
        super(props, context)
        @context_id = @self_class_root.context_id
        @child_context = { @context_id => self }
        @getChildContext = true
        @subs = []
      end

      def get_child_context
        @child_context
      end

      def render
        @props[:children]
      end

      def should_component_update?(next_props, _next_state, _context)
        if @props[:value] != next_props[:value]
          @subs.each do |subc|
            Preact._enqueue_render(subc)
          end
        end
        true
      end

      def sub(component)
        @subs << component
        component.provider_subs = @subs
      end
    end

    attr_reader :context_id
    attr_reader :Provider
    attr_accessor :value

    def initialize(default_value = nil)
      @Provider = Class.new(Provider)
      @value = default_value
      @context_id = Preact._context_id
      @Provider.instance_variable_set(:@root_context, self)
    end
  end
end
