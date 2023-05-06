class Route < Preact::Component

  self.context_type = RouterContext

  render do
    route_match = @context[:router][:matcher].call(props[:path], @context[:location])
    matches, params = props[:match] || route_match
    if matches
      component = props[:component]
      component_props = props[:component_props] || {}
      if component
        component = component.constantize if component.is_a?(String)
        Preact.create_element(component, component_props.merge!({ params: params }))
      else
        props[:children]
      end
    end
  end
end
