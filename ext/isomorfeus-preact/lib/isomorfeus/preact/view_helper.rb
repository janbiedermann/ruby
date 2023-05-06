module Preact
  module ViewHelper
    def self.included(base)
      base.include Isomorfeus::AssetManager::ViewHelper
    end

    def cached_mount_component(component_name, props = {}, skip_ssr: false, refresh: false)
      key = "#{Isomorfeus.current_user}#{component_name}#{props}"
      if !Isomorfeus.development? && !refresh
        render_result, @_ssr_styles, status = component_cache.fetch(key)
        Isomorfeus.ssr_response_status = status
        return render_result
      end
      render_result = mount_component(component_name, props, skip_ssr: skip_ssr)
      status = Isomorfeus.ssr_response_status || 200
      component_cache.store(key, render_result, ssr_styles, status) if status >= 200 && status < 300
      render_result
    end

    def mount_component(component_name, props = {}, skip_ssr: false)
      ssr_start_time = Time.now if Isomorfeus.development?
      locale = props.delete(:locale)
      location_host = props[:location_host] ? props[:location_host] : 'localhost'
      location = "#{props[:location_scheme] || 'http:'}//#{location_host}#{props[:location]}"

      rendered_tree, application_state, @_ssr_styles = Isomorfeus::TopLevel.mount_component(component_name, props, location, locale, skip_ssr)
      component_name = component_name.to_s unless component_name.is_a?(String)
      usids = if Isomorfeus.respond_to?(:current_user) && Isomorfeus.current_user && !Isomorfeus.current_user.anonymous?
                Isomorfeus.current_user.sid.to_s
              else
                nil
              end

      @_ssr_data = <<~HTML
      <script type='application/javascript'>
        ServerSideRenderingStateJSON = #{Oj.dump(application_state, mode: :strict)}
        ServerSideRenderingProps = #{Oj.dump({ env: Isomorfeus.env, component_name: component_name, props: props, hydrated: !skip_ssr, usids: usids }, mode: :strict)}
      </script>
      HTML
      puts "Preact::ViewHelper Server Side Rendering took ~#{((Time.now - ssr_start_time)*1000).to_i}ms" if Isomorfeus.development?
      rendered_tree ? rendered_tree : "SSR didn't work"
    end

    def ssr_response_status
      Isomorfeus.ssr_response_status
    end

    def ssr_data
      @_ssr_data
    end

    def ssr_styles
      @_ssr_styles || ''
    end

    private

    def component_cache
      @_component_cache ||= Isomorfeus.component_cache_init_block.call
    end
  end
end
