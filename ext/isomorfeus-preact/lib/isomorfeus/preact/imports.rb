module Isomorfeus
  module PreactImports
    def self.add
      if Dir.exist?(Isomorfeus.app_root)
        if File.exist?(File.join(Isomorfeus.app_root, 'isomorfeus_loader.rb'))
          Isomorfeus.add_web_ruby_import('isomorfeus_loader')
        end
      end
    end
  end
end
