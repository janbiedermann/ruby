module Preact::HtmlElements
  # https://html.spec.whatwg.org/multipage/indices.html#elements-3
  SUPPORTED_HTML_ELEMENTS = %w[
    a abbr address area article aside audio
    b base bdi bdo blockquote body br button
    canvas caption cite code col colgroup
    data datalist dd del details dfn dialog div dl dt
    em embed
    fieldset figcaption figure footer form
    h1 h2 h3 h4 h5 h6 head header hgroup hr html
    i iframe img input ins
    kbd
    label legend li link
    main map mark math menu meta meter
    nav noscript
    object ol optgroup option output
    p picture pre progress
    q
    rp rt ruby
    s samp script section select slot small source span strong style sub summary sup svg
    table tbody td template textarea tfoot th thead time title tr track
    u ul
    var video
    wbr
  ]

  if RUBY_ENGINE == 'opal'
    SUPPORTED_HTML_ELEMENTS.each do |element|
      define_method(element.underscore.JS.toUpperCase()) do |props = nil, &block|
        %x{
          const op = Opal.Preact;
          const opr = op.render_buffer;
          if (typeof block === 'function') op.$create_element.$$p = block.$to_proc();
          opr[opr.length-1].push(op.$create_element(element, props, nil));
        }
      end
    end
  else
    SUPPORTED_HTML_ELEMENTS.each do |element|
      define_method(element.to_s.underscore.upcase.to_sym) do |props = nil, &block|
        Preact._render_element(element, props, &block)
      end
    end
  end

  def GPE(arg, &block)
    if block_given?
      el = block.call
      return el if el
    end
    Preact.render_buffer.last.pop
  end

  def RPE(el)
    Preact.render_buffer.last << el
    nil
  end
end
