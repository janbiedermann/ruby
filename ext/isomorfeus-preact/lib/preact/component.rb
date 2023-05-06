module Preact
  class Component
    extend Preact::PropDeclarationMixin
    include Preact::HtmlElements
    include Preact::ComponentResolution

    class << self
      def inherited(base)
        base.instance_exec do
          base_module = base.to_s.deconstantize
          unless %w[LucidApp LucidComponent Preact::Component Preact::Context Fragment].include?(base_module)
            if base_module != ''
              base_module.constantize.define_singleton_method(base.to_s.demodulize) do |props = nil, &block|
                Preact._render_element(base, props, &block)
              end
            end
          end
        end
      end

      attr_accessor :context_type

      # lifecycle methods

      def component_did_catch(&block)
        define_method(:component_did_catch) do |error|
          instance_exec(error, &block)
          nil
        end
      end

      def component_did_mount(&block)
        define_method(:component_did_mount) do
          instance_exec(&block)
          nil
        end
      end

      def component_did_update(&block)
        define_method(:component_did_update) do |prev_props, prev_state, snapshot|
          instance_exec(prev_props, prev_state, snapshot, &block)
          nil
        end
      end

      def component_will_unmount(&block)
        define_method(:component_will_unmount) do
          instance_exec do
            @provider_subs.delete(self) if @provider_subs
          end
          instance_exec(&block)
          nil
        end
      end

      def get_derived_state_from_error(&block)
        define_method(:get_derived_state_from_error) do |error|
          instance_exec(error, &block)
          new_state
        end
      end

      def get_derived_state_from_props(&block)
        define_method(:get_derived_state_from_props) do |next_props, next_state|
          instance_exec(next_props, next_state, &block)
        end
      end

      def get_snapshot_before_update(&block)
        define_method(:get_snapshot_before_update) do |prev_props, prev_state|
          instance_exec(prev_props, prev_state, &block)
        end
      end

      def render(&block)
        define_method(:render) do
          pr = Preact.render_buffer
          pr << []
          block_result = instance_exec(&block)
          if Preact.is_renderable?(block_result)
            pr.pop << block_result
          else
            pr.pop
          end
        end
      end

      def should_component_update?(&block)
        define_method(:should_component_update?) do |next_props, next_state, context|
          instance_exec(next_props, next_state, context, &block)
        end
      end

      # state

      def state
        @default_state ||= {}
      end

      def set_state(update)
        state.merge!(update)
      end

      # refs

      def ref(ref_name, &block)
        @declared_refs = {} unless @declared_refs
        @declared_refs[ref_name] = block
        attr_reader ref_name
      end
    end

    attr_accessor :provider_subs
    attr_reader :context
    attr_reader :props
    attr_reader :state

    def initialize(props, context)
      @self_class = self.class

      if RUBY_ENGINE == 'opal'
        @state = `#@self_class.default_state`&.dup
        declared_refs = `#@self_class.declared_refs`
      else
        @state = @self_class.instance_variable_get(:@default_state)&.dup
        declared_refs = @self_class.instance_variable_get(:@declared_refs)
      end

      # props
      @props = props

      # state
      @state = {} unless @state

      # context
      @context = context
      @context_type = @self_class.context_type

      # refs
      declared_refs&.each do |ref_name, block|
        if block
          cmpnt = self
          instance_variable_set("@#{ref_name}".to_sym, proc { |element| cmpnt.instance_exec(element, &block) })
        else
          instance_variable_set("@#{ref_name}".to_sym, { current: nil })
        end
      end

      @method_cache = {}
    end

    def call(m)
      return @method_cache[m] if @method_cache.key?(m)
      @method_cache[m] = method(m)
    end

    def params
      @props[:params] || {}
    end

    def component_will_unmount
      @provider_subs.delete(self) if @provider_subs
    end

    def force_update(&block)
      if @_vnode
        # Set render mode so that we can differentiate where the render request
        # is coming from. We need this because forceUpdate should never call
        # should_component_update
        @_force = true
        @_renderCallbacks << block.to_n if block_given?
        Preact._enqueue_render(self)
      end
    end

    def set_state(update, &block)
      s = if @_nextState && @_nextState != @state
            @_nextState
          else
            @_nextState = @state.dup
          end
      s = {} unless s

      update = update.call(s, @props) if update.is_a?(Proc)

      if update
        s.merge!(update)
      else
        return
      end

      if @_vnode
        @_renderCallbacks << block.to_n if block_given?
        Preact._enqueue_render(self)
      end
    end
  end
end
