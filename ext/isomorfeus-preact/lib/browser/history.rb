module Browser
  class History
    if RUBY_ENGINE == 'opal'
      include Native::Wrapper

      alias_native :back
      alias_native :forward
      alias_native :go

      native_reader :length
      alias :size :length

      def push_state(state, title = '', url = `null`)
        `#@native.pushState(#{state.to_n}, #{title}, #{url})`
      end

      def replace_state(state, title = '', url = `null`)
        `#@native.replaceState(#{state.to_n}, #{title}, #{url})`
      end

      def scroll_restoration
        `#@native.scrollRestoration`
      end

      def scroll_restoration=(s)
        `#@native.scrollRestoration = #{s}`
      end

      def state
        `Opal.hash(#@native.state)`
      end
    else
      def back; end
      def forward; end
      def go(_); end

      def length
        0
      end
      alias :size :length

      def push_state(state, title = '', url = nil); end
      def replace_state(state, title = '', url = nil); end
      def scroll_restoration; end
      def scroll_restoration=(s); end

      def state
        {}
      end
    end
  end
end
