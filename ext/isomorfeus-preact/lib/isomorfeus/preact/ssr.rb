module Isomorfeus
  class PreactSSR
    def initialize(component_name, props, location, locale)
      @component = component_name.is_a?(String) ? self.class.const_get(component_name) : component_name
      @props = props
      @location = location
      @locale = locale
    end

    def render(skip_ssr)
      Isomorfeus.browser_location = Browser::Location.new(@location)
      Isomorfeus.current_locale = @locale if @locale
      NanoCSS.instance = NanoCSS.new(given_renderer: NanoCSS.global_instance.renderer.deep_dup)
      Isomorfeus.init_store
      Isomorfeus.store.clear!
      Isomorfeus.store.dispatch(type: 'I18N_MERGE', data: { locale: @locale, domain: Isomorfeus.i18n_domain })
      return '' if skip_ssr
      c = Isomorfeus.current_user
      if c.respond_to?(:reload)
        Thread.current[:isomorfeus_user] = LocalSystem.new
        begin
          c.reload
        ensure
          Thread.current[:isomorfeus_user] = c
        end
      end
      ::Preact.render_to_string(::Preact.create_element(@component, @props))
    end
  end
end
