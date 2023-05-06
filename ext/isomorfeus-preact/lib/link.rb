class Link < Preact::Component
  EVENT_PUSH_STATE = "pushState"
  EVENT_REPLACE_STATE = "replaceState"

  self.context_type = RouterContext

  def initialize(props, context)
    super(props, context)
    if RUBY_ENGINE == 'opal'
      if `typeof window !== "undefined"`
        @history = `window.history`
      else
        @history = `{ length: 1, state: null, scrollRestoration: "auto", pushState: function(e,t,n){}, replaceState: function(e,t,n){} }`
      end
    end
    @rbase = nil
  end

  def navigate
    rto = props[:to] || props[:href]
    m = props[:replace] ? EVENT_REPLACE_STATE : EVENT_PUSH_STATE
    `#@history[m](null, "", rto[0] === "~" ? rto.slice(1) : #@rbase + rto)`
    @context[:router][:callback]&.call
  end

  def handle_click(event)
    # ignores the navigation when clicked using right mouse button or
    # by holding a special modifier key: ctrl, command, win, alt, shift
    if event.ctrl? || event.meta? || event.alt? || event.shift? || event.button > 0
      event.prevent
      return
    end
    props[:on_click]&.call(event)
    event.prevent unless event.prevented?
    navigate
  end

  render do
    to = props[:to]
    href = props[:href] || to
    children = props[:children]
    @rbase = @context[:router][:base]
    extra_props = {
      # handle nested routers and absolute paths
      href: href[0] === "~" ? href.slice(1) : @rbase + href,
      on_click: call(:handle_click),
      to: nil
    }
    # wraps children in `a` if needed
    a = children.is_a?(VNode) ? children : Preact.create_element("a", props)
    Preact.clone_element(a, extra_props)
  end
end
