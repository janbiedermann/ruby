module Preact
  module PropDeclarationMixin
    def declared_props
      @declared_props ||= {}
    end

    def prop(prop_name, validate_hash = { required: true })
      validate_hash = validate_hash.to_h if validate_hash.class == Isomorfeus::Props::ValidateHashProxy
      declared_props[prop_name.to_sym] = validate_hash
    end

    def valid_prop?(prop, value)
      validate_prop(prop, value)
    rescue
      false
    end

    def valid_props?(props)
      validate_props(props)
    rescue
      false
    end

    def validate
      Isomorfeus::Props::ValidateHashProxy.new
    end

    def validate_prop(prop, value)
      return false unless declared_props.key?(prop)
      validator = Isomorfeus::Props::Validator.new(self, prop, value, declared_props[prop])
      validator.validate!
      true
    end

    def validate_props(props)
      props = {} unless props
      declared_props.each_key do |prop|
        if declared_props[prop].key?(:required) && declared_props[prop][:required] && !props.key?(prop)
          Isomorfeus.raise_error(message: "Required prop '#{prop}' not given!")
        end
      end
      result = true
      props.each do |p, v|
        r = validate_prop(p, v)
        result = false unless r
      end
      result
    end

    def validated_prop(prop, value)
      Isomorfeus.raise_error(message: "No such prop '#{prop}' declared!") unless declared_props.key?(prop)
      validator = Isomorfeus::Props::Validator.new(self, prop, value, declared_props[prop])
      validator.validated_value
    end

    def validated_props(props)
      props = {} unless props

      declared_props.each_key do |prop|
        if declared_props[prop].key?(:required) && declared_props[prop][:required] && !props.key?(prop)
          Isomorfeus.raise_error(message: "Required prop '#{prop}' not given!")
        end
        props[prop] = nil unless props.key?(prop) # let validator handle value
      end

      result = {}
      props.each do |p, v|
        result[p] = validated_prop(p, v)
      end
      result
    end
  end
end
