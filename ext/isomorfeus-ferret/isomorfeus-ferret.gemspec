# -*- encoding: utf-8 -*-
require_relative 'lib/isomorfeus/ferret/version.rb'

Gem::Specification.new do |s|
  s.name          = 'isomorfeus-ferret'
  s.version       = Isomorfeus::Ferret::VERSION

  s.authors       = ['Jan Biedermann']
  s.email         = ['jan@kursator.com']
  s.homepage      = 'https://isomorfeus.com'
  s.summary       = 'Indexed document store for Isomorfeus.'
  s.license       = 'MIT'
  s.description   = 'Indexed document store and search for Isomorfeus based on ferret.'
  s.metadata      = {
                      "github_repo" => "ssh://github.com/isomorfeus/gems",
                      "source_code_uri" => "https://github.com/isomorfeus/isomorfeus-ferret"
                    }
  s.files         = `git ls-files -- lib ext LICENSE README.md`.split("\n")
  s.require_paths = ['lib']
  s.extensions    = %w(ext/isomorfeus_ferret_ext/extconf.rb)
  s.required_ruby_version = '>= 3.0.0'

  s.add_development_dependency 'rake'
  s.add_development_dependency 'rake-compiler'
  s.add_development_dependency 'test-unit'
end
