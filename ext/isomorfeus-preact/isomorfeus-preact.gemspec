# -*- encoding: utf-8 -*-
require_relative 'lib/isomorfeus/preact/version.rb'

Gem::Specification.new do |s|
  s.name          = 'isomorfeus-preact'
  s.version       = Isomorfeus::Preact::VERSION
  s.authors       = 'Jan Biedermann'
  s.email         = 'jan@kursator.de'
  s.license       = 'MIT'
  s.homepage      = 'https://isomorfeus.com'
  s.summary       = 'Preact Components for Isomorfeus.'
  s.description   = 'Write Preact Components in Ruby, including styles and reactive data access.'
  s.metadata      = {
                      "github_repo" => "ssh://github.com/isomorfeus/gems",
                      "source_code_uri" => "https://github.com/isomorfeus/isomorfeus-project/isomorfeus-preact"
                    }
  s.files         = `git ls-files -- lib opal ext LICENSE README.md`.split("\n")
  s.require_paths = ['lib']
  s.extensions    = %w[ext/isomorfeus_preact_ext/extconf.rb]
  s.required_ruby_version = '>= 3.0.0'

  s.add_dependency 'oj', '>= 3.13.23', '< 3.15.0'
  s.add_dependency 'opal', '~> 1.7.2'
  s.add_dependency 'opal-activesupport', '~> 0.3.3'
  s.add_dependency 'isomorfeus-asset-manager', '~> 0.17.0'
  s.add_dependency 'isomorfeus-i18n', Isomorfeus::Preact::VERSION
  s.add_dependency 'isomorfeus-redux', Isomorfeus::Preact::VERSION
  s.add_development_dependency 'isomorfeus-puppetmaster', '~> 0.8.6'
  s.add_development_dependency 'rake'
  s.add_development_dependency 'rake-compiler'
  s.add_development_dependency 'rspec', '~> 3.12.0'
end
