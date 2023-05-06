class LucidComponent < Preact::Component
  include LucidI18nMixin

  class << self
    def inherited(base)
      base.context_type = LucidApplicationContext
    end

    def render(&block)
      define_method(:render) do
        pr = Preact.render_buffer
        block_result = _internal_render(pr, &block)
        if Preact.is_renderable?(block_result)
          pr.pop << block_result
        else
          pr.pop
        end
      end
    end

    def while_loading(&block)
      @while_loading_block = block
    end

    # styles

    def styles(styles_hash = nil, &block)
      styles_hash = block.call if block_given?
      @styles_hash = styles_hash if styles_hash
      @styles_hash
    end

    if RUBY_ENGINE == 'opal'
      def css_styles
        return {} unless @styles_hash
        return @css_styles if @css_styles
        rule_name = self.to_s.underscore
        i = NanoCSS.instance
        i.delete_from_sheet(rule_name)
        i.renderer[:hydrate_force_put] = true
        @css_styles = i.sheet(@styles_hash, rule_name)
      end
    else
      def css_styles
        return {} unless @styles_hash
        s = thread_css_styles
        return s if s
        rule_name = self.to_s.underscore
        i = NanoCSS.instance
        i.delete_from_sheet(rule_name)
        i.renderer[:hydrate_force_put] = true
        self.thread_css_styles = i.sheet(@styles_hash, rule_name)
      end

      def thread_css_styles_key
        @thread_css_styles_key ||= "#{self.to_s.underscore}_css_styles".to_sym
      end

      def thread_css_styles
        Thread.current[thread_css_styles_key]
      end

      def thread_css_styles=(s)
        Thread.current[thread_css_styles_key] = s
      end
    end
  end

  attr_reader :styles

  def initialize(props, context)
    super(props, context)
    @class_name = @self_class.to_s
    if RUBY_ENGINE == 'opal'
      @while_loading_block = `#@self_class.while_loading_block`
    else
      @while_loading_block = @self_class.instance_variable_get(:@while_loading_block)
    end
    # styles
    @styles = @self_class.css_styles
  end

  def _internal_render(pr, &block)
    pr << []
    outer_loading = Isomorfeus.something_loading?
    ex = nil
    begin
      block_result = instance_exec(&block)
    rescue => e
      ex = e
    end
    if Isomorfeus.something_loading?
      STDERR.puts "#{@class_name} component still loading ...\n#{ex.message}\n#{ex.backtrace&.join("\n")}" if ex && Isomorfeus.development?
      pr[pr.length-1].clear
      block_result = @while_loading_block ? instance_exec(&@while_loading_block) : Preact.create_element('div')
    elsif ex
      raise ex
    end
    Isomorfeus.something_loading! if outer_loading
    block_result
  end

  def should_component_update?(next_props, next_state, next_context)
    return true if @props != next_props || @state != next_state || @context != next_context
    false
  end

  def current_user
    Isomorfeus.current_user
  end

  def history
    Isomorfeus.browser_history
  end

  def location
    Isomorfeus.browser_location
  end

  def app_state
    @context.dig(:iso_store, :application_state)
  end

  def set_app_state(update)
    action = { type: 'APPLICATION_STATE', state: update }
    Isomorfeus.store.deferred_dispatch(action)
  end

  def class_state
    res = @context.dig(:iso_store, :class_state, @class_name)
    return res if res
    {}
  end

  def set_class_state(update)
    action = { type: 'CLASS_STATE', class: @class_name, state: update }
    Isomorfeus.store.deferred_dispatch(action)
  end

  def local_store
    LocalStore
  end

  def session_store
    SessionStore
  end

  def theme
    @context[:iso_theme]
  end
end
