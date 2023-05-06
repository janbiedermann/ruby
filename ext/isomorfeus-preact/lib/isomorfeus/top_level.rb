module Isomorfeus
  class TopLevel
    class << self
      attr_accessor :hydrated
      attr_accessor :first_pass

      if RUBY_ENGINE == 'opal'
        def do_the_mount!(init: true)
          NanoCSS.instance = NanoCSS.new({ sh: `document.getElementById('css-server-side')` })
          p = `global.ServerSideRenderingProps`
          component_name = p.JS[:component_name]
          Isomorfeus.env = p.JS[:env]
          Isomorfeus.current_user_sid_s = p.JS[:usids]
          component = nil
          begin
            component = component_name.constantize
          rescue Exception => e
            if init
              `console.warn("Deferring mount: " + #{e.message})`
              @timeout_start = Time.now unless @timeout_start
              if (Time.now - @timeout_start) < 10
                `setTimeout(Opal.Isomorfeus.TopLevel['$mount!'], 100)`
              else
                `console.error("Unable to mount '" + #{component_name} + "'!")`
              end
            else
              raise e
            end
          end
          if component
            props = `Opal.hash(p.props)`
            if init
              Isomorfeus::TopLevel.hydrated = p.JS[:hydrated]
              %x{
                var state = global.ServerSideRenderingStateJSON;
                if (state) {
                  var keys = Object.keys(state);
                  for(var i=0; i < keys.length; i++) {
                    if (Object.keys(state[keys[i]]).length > 0) {
                      #{Isomorfeus.store.dispatch({ type: `keys[i].toUpperCase()`, set_state: Hash.recursive_new(`state[keys[i]]`) })}
                    }
                  }
                }
              }
              Isomorfeus.execute_init_after_store_classes
            end
            begin
              Isomorfeus::TopLevel.first_pass = true
              result = Isomorfeus::TopLevel.mount_component(component, props, `document.body`, Isomorfeus::TopLevel.hydrated)
              Isomorfeus::TopLevel.first_pass = false
              @tried_another_time = false
              result
            rescue Exception => e
              if init
                Isomorfeus::TopLevel.first_pass = false
                if !@tried_another_time
                  @tried_another_time = true
                  `console.warn("Deferring mount: " + #{e.message})`
                  `console.error(#{e.backtrace.join("\n")})`
                  `setTimeout(Opal.Isomorfeus.TopLevel['$mount!'], 250)`
                else
                  `console.error("Unable to mount '" + #{component_name} + "'! Error: " + #{e.message} + "!")`
                  `console.error(#{e.backtrace.join("\n")})`
                end
              else
                raise e
              end
            end
          end
        end

        def mount!
          Isomorfeus.init
          Isomorfeus::TopLevel.on_ready do
            Isomorfeus::TopLevel.do_the_mount!
          end
        end

        def on_ready(&block)
          %x{
            function run() { block.$call() };
            function ready_fun(fn) {
              if (document.readyState === "complete" || document.readyState === "interactive") {
                setTimeout(fn, 1);
              } else {
                document.addEventListener("DOMContentLoaded", fn);
              }
            }
            ready_fun(run);
          }
        end

        def on_ready_mount(component, props = nil, element_query = nil)
          # init in case it hasn't been run yet
          Isomorfeus.init
          on_ready do
            Isomorfeus::TopLevel.mount_component(component, props, element_query)
          end
        end

        def mount_component(component, props, element_or_query, hydrated = false)
          if `(element_or_query instanceof HTMLElement)`
            element = element_or_query
          elsif `(typeof element_or_query === 'string')` || element_or_query.is_a?(String)
            element = `document.body.querySelector(element_or_query)`
          elsif element_or_query.is_a?(Browser::Element)
            element = element_or_query.to_n
          else
            element = element_or_query
          end
          raise "Element is required!" unless element
          top = ::Preact.create_element(component, props)
          hydrated ? ::Preact.hydrate(top, element) : ::Preact.render(top, element)
          Isomorfeus.top_component = top
        end
      else
        def mount_component(component, props, location, locale, skip_ssr = false)
          rendered_tree = Isomorfeus::PreactSSR.new(component, props, location, locale).render(skip_ssr)
          [rendered_tree, Isomorfeus.store.get_state, NanoCSS.instance.renderer[:raw]]
        end
      end
    end
  end
end
