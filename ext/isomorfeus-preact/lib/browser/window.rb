module Browser
  module Window
    extend DelegateNative
    extend EventTarget

    @native = `window`

    module_function

    if `#@native.requestAnimationFrame !== undefined`
      # Add the given block to the current iteration of the event loop. If this
      # is called from another `animation_frame` call, the block is run in the
      # following iteration of the event loop.
      def animation_frame &block
        `requestAnimationFrame(function(now) { #{block.call `now`} })`
        self
      end
    else
      def animation_frame &block
        after(0, &block)
        self
      end
    end

    # Run the given block every `duration` seconds
    #
    # @param duration [Numeric] the number of seconds between runs
    def set_interval duration, &block
      `setInterval(function() { #{block.call} }, duration)`
    end

    def set_timeout duration, &block
      `setTimeout(function() { #{block.call} }, duration)`
    end

    # return [History] the browser's History object
    def history
      Browser::History.new(`window.history`)
    end

    # @return [Location] the browser's Location object
    def location
      Browser::Location.new(`window.location`)
    end

    # Scroll to the specified (x,y) coordinates
    def scroll x, y
      `window.scrollTo(x, y)`
    end
  end
end
