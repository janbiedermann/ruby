module Preact::ComponentResolution
  if RUBY_ENGINE == 'opal'
    unless method_defined?(:_preact_component_resolution_original_method_missing)
      alias _preact_component_resolution_original_method_missing method_missing
    end

    def method_missing(component_name, *args, &block)
      # Further on it must check for modules, because $const_get does not take
      # the full nesting into account, as usually its called via $$ with the
      # nesting provided by the compiler.
      %x{
        let constant;
        let sc = self.$class();
        // disable stack traces here for improved performance
        // if const cannot be found, the orignal method_mising will throw with stacktrace
        let orig_est = Opal.config.enable_stack_trace;
        Opal.config.enable_stack_trace = false;
        try {
          // disable the const cache for development, so that always the latest constant
          // version is used to make hot reloading reliable
          if (!Opal.Isomorfeus.development) {
            if (!self.$$iso_preact_const_cache) { self.$$iso_preact_const_cache = {}; }
            constant = self.$$iso_preact_const_cache[component_name];
          }
          if (!constant) { constant = sc.$const_get(component_name); }
        } catch(err) {
          let module_names;
          if (sc.$$full_name) { module_names = sc.$$full_name.split("::"); }
          else { module_names = sc.$to_s().split("::"); }
          let module_name;
          for (let i = module_names.length - 1; i > 0; i--) {
            module_name = module_names.slice(0, i).join('::');
            try {
              constant = sc.$const_get(module_name).$const_get(component_name, false);
              break;
            } catch(err) { }
          }
        } finally {
          Opal.config.enable_stack_trace = orig_est;
        }
        if (constant) {
          if (!Opal.Isomorfeus.development && !self.$$iso_preact_const_cache[component_name]) {
            self.$$iso_preact_const_cache[component_name] = constant;
          }
          let last = args[args.length-1];
          #{`Opal.Preact`._render_element(`constant`, `(last === undefined || last === null) ? nil : last`, &block)};
          return nil;
        }
        return #{_preact_component_resolution_original_method_missing(component_name, *args, block)};
      }
    end
  else
    unless method_defined?(:_preact_component_resolution_original_method_missing)
      alias _preact_component_resolution_original_method_missing method_missing
    end

    def method_missing(component_name, *args, &block)
      # Further on it must check for modules, because const_get does not take
      # the full nesting into account
      constant = nil
      begin
        if !Isomorfeus.development?
          @iso_preact_const_cache = {} unless @iso_preact_const_cache
          constant = @iso_preact_const_cache[component_name]
        end
        constant = self.class.const_get(component_name) unless constant
      rescue
        module_names = self.class.to_s.split('::')
        module_names.each_index do |i|
          module_name = module_names[0..i].join('::')
          constant = self.class.const_get(module_name).const_get(component_name, false) rescue nil
          break if constant
        end
      end

      if constant
        if !Isomorfeus.development? && @iso_preact_const_cache.key?(component_name)
          @iso_preact_const_cache[component_name] = constant
        end
        Preact._render_element(constant, args.last, &block)
        nil
      else
        _preact_component_resolution_original_method_missing(component_name, *args, block)
      end
    end
  end
end
