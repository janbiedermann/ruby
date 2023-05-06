/*
  class VNode
    attr_accessor :__c
    attr_reader :key
    attr_reader :props
    attr_reader :ref
    attr_reader :type

    def initialize(type, props, key, ref)
      @type = type
      @props = props
      @key = key
      @ref = ref
    end
  end
*/

typedef struct VNode {
  VALUE component;
  VALUE key;
  VALUE props;
  VALUE ref;
  VALUE type;
} VNode;
