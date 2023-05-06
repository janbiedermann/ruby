class LucidApp < LucidComponent
  class << self
    def inherited(base)
      base.context_type = nil
    end

    def render(&block)
      define_method(:render) do
        pr = Preact.render_buffer
        block_result = _internal_render(pr, &block)
        children = pr.pop
        if Preact.is_renderable?(block_result)
          children << block_result
        end
        Preact.create_element(LucidApplicationContext.Provider, state[:_app_ctx], children)
      end
    end

    def component_will_unmount(&block)
      define_method(:component_will_unmount) do
        instance_exec do
          Isomorfeus.store.unsubscribe(@unsubscriber)
        end
        instance_exec(&block)
        nil
      end
    end

    # theme

    def theme(theme_hash = nil, &block)
      theme_hash = block.call if block_given?
      @theme_hash = theme_hash if theme_hash
      @theme_hash
    end

    if RUBY_ENGINE == 'opal'
      def css_theme
        return {} unless @theme_hash
        return @css_theme if @css_theme
        component_name = self.to_s
        rule_name = component_name.gsub(':', '_')
        i = NanoCSS.instance
        i.delete_from_sheet(rule_name)
        i.renderer[:hydrate_force_put] = true
        @css_theme = i.sheet(@theme_hash, rule_name)
      end
    else
      def css_theme
        return {} unless @theme_hash
        t = thread_css_theme
        return t if t
        component_name = self.to_s
        rule_name = component_name.gsub(':', '_')
        i = NanoCSS.instance
        i.delete_from_sheet(rule_name)
        i.renderer[:hydrate_force_put] = true
        self.thread_css_theme = i.sheet(@theme_hash, rule_name)
      end

      def thread_css_theme_key
        @thread_css_theme_key ||= "#{self.to_s.underscore}_css_theme".to_sym
      end

      def thread_css_theme
        Thread.current[thread_css_theme_key]
      end

      def thread_css_theme=(s)
        Thread.current[thread_css_theme_key] = s
      end
    end
  end

  attr_reader :theme

  def initialize(props, context)
    super(props, context)

    # theme
    @theme = @self_class.css_theme

    @state.merge!({ _app_ctx: { value: {
      iso_location: props[:location],
      iso_store: Isomorfeus.store.get_state,
      iso_theme: @theme }}})

    if RUBY_ENGINE == 'opal'
      lai = self
      @unsubscriber = Isomorfeus.store.subscribe do
        lai.set_state({ _app_ctx: { value: {
          iso_location: lai.props[:location],
          iso_store: Isomorfeus.store.get_state,
          iso_theme: lai.theme }}})
      end
    end
  end

  def app_state
    @state.dig(:_app_ctx, :value, :iso_store, :application_state)
  end

  def class_state
    res = @state.dig(:_app_ctx, :value, :iso_store, :class_state, @class_name)
    return res if res
    {}
  end

  def should_component_update?(next_props, next_state, _next_context)
    return true if @props != next_props || @state != next_state
    false
  end

  if RUBY_ENGINE == 'opal'
    def component_will_unmount
      Isomorfeus.store.unsubscribe(@unsubscriber)
    end
  end
end
