class NanoCSS
  KEBAB_REGEX = /[A-Z]/
  UNITLESS_NUMBER_PROPS = %w[animation-iteration-count border-image-outset border-image-slice
    border-image-width box-flex box-flex-group box-ordinal-group column-count columns flex
    flex-grow flex-positive flex-shrink flex-negative flex-order grid-row grid-row-end
    grid-row-span grid-row-start grid-column grid-column-end grid-column-span grid-column-start
    font-weight line-clamp line-height opacity order orphans tabSize widows z-index zoom
    fill-opacity flood-opacity stop-opacity stroke-dasharray stroke-dashoffset stroke-miterlimit
    stroke-opacity stroke-width] # from fill-opacity onwards is for SVG

  class << self
    if RUBY_ENGINE == 'opal'
      def instance
        @instance
      end

      def instance=(i)
        @instance = i
      end
    else
      def instance
        i = Thread.current[:@_isomorfeus_preact_nanocss_instance]
        return i if i
        @global_instance
      end

      def instance=(i)
        @global_instance = i unless @global_instance
        Thread.current[:@_isomorfeus_preact_nanocss_instance] = i
      end

      def global_instance
        @global_instance
      end
    end
  end

  @@unitless_css_properties = nil

  attr_reader :renderer

  def initialize(config = nil, given_renderer: nil)
    config = {} unless config

    if given_renderer
      @renderer = given_renderer
    else
      @renderer = { raw: '', pfx: '_', hydrate_force_put: false, prefixes: ['-webkit-', '-moz-', '-o-', ''] }
      @renderer.merge!(config)
    end

    @hydrated = {}

    if RUBY_ENGINE == 'opal'
      unless @renderer.key?(:sh)
        e = `document.createElement('style')`
        @renderer[:sh] = e
        `document.head.appendChild(e)`
      end

      unless @renderer.key?(:kh)
        e = `document.createElement('style')`
        @renderer[:ksh] = e
        `document.head.appendChild(e)`
      end

      hydrate(@renderer[:sh])
    end

    unless @@unitless_css_properties
      @@unitless_css_properties = {}
      UNITLESS_NUMBER_PROPS.each do |prop|
        @@unitless_css_properties[prop] = 1
        @@unitless_css_properties["-webkit-#{prop}"] = 1
        @@unitless_css_properties["-ms-#{prop}"] = 1
        @@unitless_css_properties["-moz-#{prop}"] = 1
        @@unitless_css_properties["-o-#{prop}"] = 1
      end
    end

    unless given_renderer
      put('', { '@keyframes fadein' => { from: { opacity: 0 }, to: { opacity: 1 }},
                '.fade_in' => { animation: 'fadein .4s linear' }})
      put('', { '@keyframes fadeout' => { from: { opacity: 1 }, to: { opacity: 0 }},
                '.fade_out' => { animation: 'fadeout .3s linear', 'animation-fill-mode' => 'forwards' }})
    end
  end

  def decl(key, value)
    key = kebab(key)

    if value.is_a?(Numeric) && !@@unitless_css_properties.key?(key)
      "#{key}:#{value}px;"
    else
      "#{key}:#{value};"
    end
  end

  def hash(obj)
    hash_str(JSON.dump(obj))
  end

  def hash_str(str)
    h = 5381
    str.each_codepoint do |cp|
      h = (h * 33) ^ cp
    end
    "_#{h.abs.to_s(36)}"
  end

  def kebab(prop)
    prop.to_s.gsub(KEBAB_REGEX, '-$&').downcase.to_sym
  end

  def put(css_selector, decls, atrule = nil)
    return if RUBY_ENGINE == 'opal' && !@renderer[:hydrate_force_put] && @hydrated.key?(css_selector)

    str = ''
    postponed = []

    decls.each do |prop, value|
      if value.is_a?(Hash) && !value.is_a?(Array)
        postponed << prop
      else
        str += decl(prop, value)
      end
    end

    unless str.empty?
      str = "#{css_selector}{#{str}}"
      put_raw(atrule ? "#{atrule}{#{str}}" : str)
    end

    postponed.each do |prop|
      if prop[0] === '@' && prop != '@font-face'
        put_at(css_selector, decls[prop], prop)
      else
        put(selector(css_selector, prop), decls[prop], atrule)
      end
    end
  end

  def put_at(_, keyframes, prelude)
    if prelude[1] == 'k'
      str = ''
      keyframes.each do |keyframe, decls|
        str_decls = ''
        decls.each do |prop, value|
          str_decls += decl(prop, value)
        end
        str += "#{keyframe}{#{str_decls}}"
      end

      @renderer[:prefixes].each do |prefix|
        raw_key_frames = "#{prelude.sub('@keyframes', "@#{prefix}keyframes")}{#{str}}"
        if RUBY_ENGINE == 'opal'
          ksh = @renderer[:ksh]
          `ksh.appendChild(document.createTextNode(raw_key_frames))`
        else
          put_raw(raw_key_frames)
        end
      end

      return
    end
    put(nil, keyframes, prelude)
  end

  if RUBY_ENGINE == 'opal'
    def put_raw(raw_css_rule)
      # .insertRule() is faster than .appendChild(), that's why we use it in PROD.
      # But CSS injected using .insertRule() is not displayed in Chrome Devtools
      sheet = @renderer[:sh].JS[:sheet]
      # Unknown pseudo-selectors will throw, this try/catch swallows all errors.
      `sheet.insertRule(raw_css_rule, sheet.cssRules.length)` rescue nil
    end
  else
    def put_raw(raw_css_rule)
      @renderer[:raw] << raw_css_rule
    end
  end

  # addons

  # rule

  def rule(css, block = nil)
    block = block || hash(css)
    block = "#{@renderer[:pfx]}#{block}"
    put(".#{block}", css)

    " #{block}"
  end

  # sheet

  def delete_from_sheet(rule_name)
    selector_rule_name = "._#{rule_name}-"
    if renderer[:sh] && renderer[:sh].JS[:sheet]
      sheet = renderer[:sh].JS[:sheet]
      css_rules = sheet.JS[:cssRules]
      %x{
        let i = 0;
        for(i=0; i<css_rules.length; i++) {
          if (css_rules[i].cssText.includes(selector_rule_name)) {
            sheet.deleteRule(i);
          }
        }
      }
    end
  end

  def on_element_modifier(element_modifier, map, block, result)
    result[element_modifier] = rule(map[element_modifier], "#{block}-#{element_modifier}")
  end

  def sheet(map, block = nil)
    result = {}
    block = hash(map) unless block
    map.each_key do |element_modifier|
      on_element_modifier(element_modifier, map, block, result)
    end
    result
  end

  # nesting

  def selector(parent_selectors, css_selector)
    parent_selectors = '' if parent_selectors.include?(':global')
    parents = parent_selectors.split(',')
    selectors = css_selector.split(',')
    result = []

    selectors.each do |sel|
      pos = sel.index('&')

      if pos
        if parents.empty?
          replaced_selector = sel.gsub(/&/, parent)
          result << replaced_selector
        else
          parents.each do |parent|
            replaced_selector = sel.gsub(/&/, parent)
            result << replaced_selector
          end
        end
      else
        if parents.empty?
          result << sel
        else
          parents.each do |parent|
            result << "#{parent} #{sel}"
          end
        end
      end
    end

    return result.join(',')
  end

  # hydrate

  def hydrate(sh)
    css_rules = sh.JS[:cssRules] || sh.JS[:sheet].JS[:cssRules]

    %x{
      let i = 0;
      let st;
      for(i=0; i<css_rules.length; i++) {
        st = css_rules[i].selectorText;
        if (st) { #{@hydrated[`st`] = 1}; }
      }
    }
  end


  # global

  def global(css)
    put('', css)
  end

  # keyframes

  def keyframes(keyframes, cblock = nil)
    block = hash(keyframes) unless block
    block = @renderer[:pfx] + block
    put_at('', keyframes, '@keyframes' + block)
    block
  end
end
