module Browser
  module DelegateNative
    # Provides a default initializer. This should be overridden in all but the
    # simplest cases.
    def initialize native
      @native = native
    end

    def [](property)
      method_missing(property)
    end

    # Fall back to native properties. If the message sent to this element is not
    # recognized, it checks to see if it is a property of the native element. It
    # also checks for variations of the message name, such as:
    #
    #   :supported? => [:supported, :isSupported]
    #
    # If a property with the specified message name is found and it is a
    # function, that function is invoked with `args`. Otherwise, the property
    # is returned as is.
    def method_missing message, *args, &block
      if message.end_with? '='
        message = message.chop
        property_name = property_for_message(message)
        arg = args[0]
        arg = arg.to_n if `arg && typeof arg.$to_n === 'function'`
        return `#@native[#{property_name}] = arg`
      else
        property_name = property_for_message(message)
        %x{
          let value = #@native[#{property_name}];
          let type = typeof(value);
          if (type === 'undefined') { return #{super}; }
          try {
            if (type === 'function') {
              #{args.map! { |arg| `arg && typeof arg.$to_n === 'function'` ? arg.to_n : arg }}
              value = value.apply(#@native, args);
            }
            if (value instanceof HTMLCollection || value instanceof NodeList) {
              let a = [];
              for(let i=0; i<value.length; i++) {
                a[i] = #{Browser::Element.new(`value.item(i)`)};
              }
              value = a;
            } else if (value instanceof HTMLElement || value instanceof SVGElement) {
              value = #{Browser::Element.new(`value`)};
            } else if (value instanceof Event) {
              value = #{Browser::Event.new(`value`)};
            } else if (value === null || type === 'undefined' || (type === 'number' && isNaN(value))) {
              value = nil;
            }
            return value;
          } catch { return value; }
        }
      end
    end

    def respond_to_missing? message, include_all
      return true if message.end_with? '='
      property_name = property_for_message(message)
      return true if `#{property_name} in #@native`
      false
    end

    def property_for_message(message)
      %x{
        let camel_cased_message;
        if (typeof(#@native[message]) !== 'undefined') { camel_cased_message = message; }
        else { camel_cased_message = #{message.camelize(:lower)} }

        if (camel_cased_message.endsWith('?')) {
          camel_cased_message = camel_cased_message.substring(0, camel_cased_message.length - 2);
          if (typeof(#@native[camel_cased_message]) === 'undefined') {
            camel_cased_message = 'is' + camel_cased_message[0].toUpperCase() + camel_cased_message.substring(0, camel_cased_message.length - 1);
          }
        }
        return camel_cased_message
      }
    end
  end
end
