module Browser
  module Document
    extend DelegateNative
    extend EventTarget

    @native = `document`

    module_function

    # @return [Browser::Element] the head element of the current document
    def head
      @head ||= Element.new(`#@native.head`)
    end

    # @return [Browser::Element] the body element of the current document
    def body
      @body ||= Element.new(`#@native.body`)
    end

    # @return [Browser::Element?] the first element that matches the given CSS
    #   selector or `nil` if no elements match
    def [] css
      native = `#@native.querySelector(css)`
      if `#{native} === null`
        nil
      else
        Element.new(native)
      end
    end
  end
end
