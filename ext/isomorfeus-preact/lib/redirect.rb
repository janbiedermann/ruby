class Redirect < Preact::Component
  EVENT_PUSH_STATE = "pushState"
  EVENT_REPLACE_STATE = "replaceState"

  self.context_type = RouterContext

  def initialize(props, context)
    super(props, context)
    if RUBY_ENGINE == 'opal'
      @history = if `typeof window !== "undefined"`
                    `window.history`
                else
                    `{ length: 1, state: null, scrollRestoration: "auto", pushState: function(e,t,n){}, replaceState: function(e,t,n){} }`
                end
      @base = ''
    end
  end

  def navigate
    if RUBY_ENGINE == 'opal'
      to = props[:to] || props[:href]
      m = props[:replace] ? EVENT_REPLACE_STATE : EVENT_PUSH_STATE
      `#@history[m](null, "", to[0] === "~" ? to.slice(1) : #@base + to)`
      context[:router][:callback].call if context[:router].key?(:callback)
    end
  end

  render do
    if RUBY_ENGINE == 'opal'
      @base = context[:router][:base] if context[:router].key?(:base)
      navigate
    end
    nil
  end
end
