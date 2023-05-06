class Switch < Preact::Component

  self.context_type = RouterContext

  def initialize(props, context)
    super(props, context)
    if RUBY_ENGINE == 'opal'
      @browser = `typeof window !== "undefined"` ? true : false
    else
      @browser = false
    end
  end

  if RUBY_ENGINE == 'opal'
    def flatten(children)
      if children.is_a?(Array)
        children.compact!
        children.map! do |child|
          if child.is_a?(Array)
            flatten(child)
          elsif `child.type === Opal.Fragment`
            flatten(child.JS[:props][:children])
          else
            child
          end
        end
      else
        [children]
      end
    end
  else
    def flatten(children)
      if children.is_a?(Array)
        children.compact!
        children.map! do |child|
          if child.is_a?(Array)
            flatten(child)
          elsif child.type == ::Fragment
            flatten(child[:props][:children])
          else
            child
          end
        end
      else
        [children]
      end
    end
  end

  render do
    router = @context[:router]
    base = router[:base]
    matcher = router[:matcher]
    originalLocation = router[:originalLocation]
    children = flatten(props[:children])
    location = props[:location]
    child = nil
    if RUBY_ENGINE == 'opal'
      children.each do |vnode|
        match = nil
        # we don't require an element to be of type Route,
        # but we do require it to contain a truthy `path` prop.
        # this allows to use different components that wrap Route
        # inside of a switch, for example <AnimatedRoute />.
        if vnode.is_a?(VNode) && (match = (p = vnode.JS[:props][:path]) ? matcher.call(p, location || @context[:location]) : [true, {}])[0]
          child = Preact.clone_element(vnode, { match: match })
          break
        end
      end
    else
      children.each do |vnode|
        match = nil
        if vnode.is_a?(VNode) && (match = (p = vnode.props[:path]) ? matcher.call(p, location || @context[:location]) : [true, {}])[0]
          child = Preact.clone_element(vnode, { match: match })
          break
        end
      end
    end
    child
  end
end
