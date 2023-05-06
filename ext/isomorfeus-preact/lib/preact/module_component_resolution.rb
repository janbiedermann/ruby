class Module
  if RUBY_ENGINE == 'opal'
    unless method_defined?(:_preact_component_class_resolution_original_method_missing)
      alias _preact_component_class_resolution_original_method_missing method_missing
    end

    # this is required for autoloading support, as the component may not be loaded and so its method is not registered.
    # must load it first, done by const_get, and next time the method will be there.
    def method_missing(component_name, *args, &block)
      # check for ruby component and render it
      # otherwise pass on method missing
      %x{
        let constant = null;
        let c = component_name[0];
        if (c == c.toUpperCase()) {
          // disable stack traces here for improved performance
          // if const cannot be found, the orignal method_mising will throw with stacktrace
          Opal.config.enable_stack_trace = false;
          try {
            // disable the const cache for development, so that always the latest constant
            // version is used to make hot reloading reliable
            if (!Opal.Isomorfeus.development) {
              if (!self.$$iso_preact_const_cache) { self.$$iso_preact_const_cache = {}; }
              constant = self.$$iso_preact_const_cache[component_name];
            }
            constant = self.$const_get(component_name);
          } catch(err) {
            // nothing
          } finally {
            Opal.config.enable_stack_trace = true;
          }
          if (constant) {
            if (!Opal.Isomorfeus.development && !self.$$iso_preact_const_cache[component_name]) {
              self.$$iso_preact_const_cache[component_name] = constant;
            }
            let last = args[args.length-1];
            #{`Opal.Preact`._render_element(`constant`, `(last === undefined || last === null) ? nil : last`, &block)};
            return nil;
          }
        }
        return #{_preact_component_class_resolution_original_method_missing(component_name, *args, block)};
      }
    end
  else
    unless method_defined?(:_preact_component_class_resolution_original_method_missing)
      alias _preact_component_class_resolution_original_method_missing method_missing
    end

    # this is required for autoloading support, as the component may not be loaded and so its method is not registered.
    # must load it first, done by const_get, and next time the method will be there.
    def method_missing(component_name, *args, &block)
      # check for ruby component and render it
      # otherwise pass on method missing
      c = component_name.to_s[0]
      if c == c.upcase
        if !Isomorfeus.development?
          @iso_preact_const_cache = {} unless @iso_preact_const_cache
          constant = @iso_preact_const_cache[component_name]
        end
        unless constant
          constant = const_get(component_name) rescue nil
        end
        if constant && constant.respond_to?(:ancestors) && constant.ancestors.include?(Preact::Component)
          if !Isomorfeus.development? && @iso_preact_const_cache.key?(component_name)
            @iso_preact_const_cache[component_name] = constant
          end
          Preact._render_element(constant, args.last, &block)
          return nil
        end
      end
      _preact_component_class_resolution_original_method_missing(component_name, *args, block)
    end
  end
end
