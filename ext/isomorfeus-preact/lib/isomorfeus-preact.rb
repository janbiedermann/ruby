require 'base64'
require 'stringio'
if RUBY_ENGINE == 'opal'
  require 'iso_uri'
else
  require 'uri'
  require 'oj'
end
require 'data_uri'
require 'isomorfeus-transport'
require 'isomorfeus-redux'
require 'isomorfeus-i18n'
require 'isomorfeus/preact/config'
require 'browser/history'
require 'browser/location'

if RUBY_ENGINE == 'opal'
  require 'browser/event'
  require 'browser/event_target'
  require 'browser/delegate_native'
  require 'browser/element'
  require 'browser/document'
  require 'browser/window'

  if `window?.history`
    Isomorfeus.browser_history = Browser::History.new(`window.history`)
    Isomorfeus.browser_location = Browser::Location.new(`window.location`)
  end
else
  Isomorfeus.browser_history = Browser::History.new
  require 'isomorfeus_preact_ext'
end

# allow mounting of components
require 'isomorfeus/top_level'

# nanocss
require 'nano_css'

# preact
require 'preact'

# props
require 'isomorfeus/props/validate_hash_proxy'
require 'isomorfeus/props/validator'
require 'preact/prop_declaration_mixin'

# component resolution
require 'preact/component_resolution'
require 'preact/module_component_resolution'

# HTML Elements and Fragment support
require 'preact/html_elements'
# require 'preact/svg_elements' # optional
# require 'preact/math_ml_elements' # optional
# Component
require 'preact/component'
# Context
require 'preact/context'

Preact::Context.create_application_contexts

# Router Components
require 'link'
require 'redirect'
require 'route'
require 'router'
require 'switch'

# LucidComponent
require 'lucid_component'

# LucidApp
require 'lucid_app'

if RUBY_ENGINE == 'opal'
  # init auto loader
  Isomorfeus.zeitwerk.push_dir('components')
else
  require 'isomorfeus/preact/version'

  if Isomorfeus.development?
    require 'net/http'
    Isomorfeus.ssr_hot_asset_url = 'http://localhost:3036/assets/'
  end

  NanoCSS.instance = NanoCSS.new(nil)

  # cache
  require 'isomorfeus/preact/thread_local_component_cache'

  require 'isomorfeus/preact/ssr'
  require 'isomorfeus/preact/view_helper'

  Isomorfeus.component_cache_init do
    Isomorfeus::ThreadLocalComponentCache.new
  end

  require 'iso_opal'
  Opal.append_path(__dir__) unless IsoOpal.paths.include?(__dir__)
  path = File.expand_path(File.join(__dir__, '..', 'opal'))
  Opal.append_path(path) unless IsoOpal.paths.include?(path)

  Isomorfeus.zeitwerk.push_dir(File.expand_path(File.join('app', 'components')))

  require 'isomorfeus/preact/imports'
  Isomorfeus::PreactImports.add
end
