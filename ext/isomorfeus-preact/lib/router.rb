class Router < Preact::Component
  EVENT_POP_STATE = "popstate"
  EVENT_PUSH_STATE = "pushState"
  EVENT_REPLACE_STATE = "replaceState"
  EVENTS = [EVENT_POP_STATE, EVENT_PUSH_STATE, EVENT_REPLACE_STATE]

  def initialize(props, context)
    super(props, context)
    @state = { update: 0 }
    @router = nil
    if RUBY_ENGINE == 'opal'
      @browser = `typeof window !== "undefined"` ? true : false
      rtr = self
      @check_for_updates_n = `function() { #{rtr.call(:check_for_updates).call}; }`
    else
      @browser = false
    end
    @rbase = nil
    @original_location = nil
    @prev_hash = nil
  end

  def build_router(props, values)
    { base: "",
      matcher: make_matcher(),
      original_location: "/",
      callback: call(:check_for_updates)
    }.merge(props, values)
  end

  def check_for_updates
    pathname = @browser ? current_pathname(@rbase) : current_pathname(@rbase, @original_location)
    search = @browser ? `location.search` : ""
    hash = pathname + search;
    if @prev_hash != hash
      @prev_hash = hash
      set_state({ update: state[:update] + 1 })
    end
  end

  def current_pathname(base, path)
    unless path
      path =  @browser ? `location.pathname` : ""
    end
    base = "" unless base
    path.downcase.start_with?(base.downcase) ? (path[base.size..-1] || "/") : "~" + path
  end

  def escape_rx(str)
    str.gsub(/([.+*?=^!:${}()\[\]\|\/\\])/, "\\1")
  end

  # returns a segment representation in Regexp based on flags
  # adapted and simplified version from path-to-regexp sources
  def rx_for_segment(repeat, optional, prefix)
    capture = repeat ? "((?:[^\\/]+?)(?:\\/(?:[^\\/]+?))*)" : "([^\\/]+?)"
    capture = "(?:\\/" + capture + ")" if (optional && prefix)
    capture + (optional ? "?" : "")
  end

  def path_to_regexp(pattern)
    group_rx = /:([A-Za-z0-9_]+)([?+*]?)/

    pos = 0
    keys = []
    result = ""

    while (match = group_rx.match(pattern, pos))
      mod = match[2]
      segment = match[1]

      # :foo  [1]      (  )
      # :foo? [0 - 1]  ( o)
      # :foo+ [1 - ∞]  (r )
      # :foo* [0 - ∞]  (ro)
      repeat = mod === "+" || mod === "*"
      optional = mod === "?" || mod === "*"
      prefix = optional && pattern[match.begin(0) - 1] == "/" ? 1 : 0

      prev = pattern[pos...(match.begin(0) - prefix)]
      keys.push({ name: segment.to_sym })
      pos = match.end(0)
      if RUBY_ENGINE == 'opal'
        result += "#{escape_rx(prev)}#{rx_for_segment(repeat, optional, prefix)}"
      else
        result << "#{escape_rx(prev)}#{rx_for_segment(repeat, optional, prefix)}"
      end
    end

    if RUBY_ENGINE == 'opal'
      result += escape_rx(pattern[pos..-1])
    else
      result << escape_rx(pattern[pos..-1])
    end
    [ Regexp.new("^" + result + "(?:\\/)?$", Regexp::IGNORECASE), keys]
  end

  def get_regexp(cache, pattern)
    # obtains a cached regexp version of the pattern
    cache[pattern] || (cache[pattern] = path_to_regexp(pattern))
  end

  def make_matcher
    cache = {}

    proc do |pattern, path|
      regexp, keys = get_regexp(cache, pattern || "")
      out = regexp.match(path)
      if !out
        [false, nil]
      else
        # formats an object with matched params
        i = 0
        params = keys.reduce({}) do |p, key|
          p[key[:name]] = out[i += 1]
          p
        end
        [true, params]
      end
    end
  end

  render do
    @router = build_router(props, { original_location: @context[:iso_location] }) if !@router
    @rbase = @router[:base]
    @original_location = @router[:original_location]
    loc = @browser ? current_pathname(@rbase) : current_pathname(@rbase, @original_location)
    @prev_hash = @browser ? loc + `location.search` : loc
    Preact.create_element(RouterContext.Provider, { value: { router: @router, location: loc }, children: props[:children] })
  end

  component_did_mount do
    EVENTS.each do |event|
      `window`.JS.addEventListener(event, @check_for_updates_n)
    end
    check_for_updates
  end

  component_will_unmount do
    EVENTS.each do |event|
      `window`.JS.removeEventListener(event, @check_for_updates_n)
    end
  end
end
Router.context_type = LucidApplicationContext
