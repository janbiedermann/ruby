module Isomorfeus
  module Ferret
    FIELD_TYPES = %w(integer float string byte).map{|t| t.to_sym}

    class BlankSlate < BasicObject
    end

    # The FieldSymbolMethods module contains the methods that are added to both
    # the Symbol class and the FieldSymbol class. These methods allow you to
    # easily set the type of a field by calling a method on a symbol.
    #
    # Right now this is only useful for Sorting and grouping, but some day Ferret
    # may have typed fields, in which case these this methods will come in handy.
    #
    # The available types are specified in Ferret::FIELD_TYPES.
    #
    # == Examples
    #
    #   index.search(query, :sort => :title.string.desc)
    #
    #   index.search(query, :sort => [:price.float, :count.integer.desc])
    #   
    #   index.search(query, :group_by => :catalogue.string)
    #
    # == Note
    #
    # If you set the field type multiple times, the last type specified will be
    # the type used. For example;
    #
    #   puts :title.integer.float.byte.string.type.inspect # => :string
    #
    # Calling #desc twice will set desc? to false
    #
    #   puts :title.desc?           # => false
    #   puts :title.desc.desc?      # => true
    #   puts :title.desc.desc.desc? # => false
    module FieldSymbolMethods
      FIELD_TYPES.each do |method|
        define_method(method) do
          fsym = FieldSymbol.new(self, respond_to?(:desc?) ? desc? : false)
          fsym.type = method
          fsym
        end
      end
        
      # Set a field to be a descending field. This only makes sense in sort
      # specifications.
      def desc
        fsym = FieldSymbol.new(self, respond_to?(:desc?) ? !desc? : true)
        fsym.type = respond_to?(:type) ? type : nil
        fsym
      end

      # Return whether or not this field should be a descending field
      def desc?
        self.class == FieldSymbol and @desc == true
      end

      # Return the type of this field
      def type
        self.class == FieldSymbol ? @type : nil
      end
    end

    # See FieldSymbolMethods
    class FieldSymbol < BlankSlate
      include FieldSymbolMethods
      
      def initialize(symbol, desc = false)
        @symbol = symbol
        @desc = desc
      end

      def method_missing(method, *args)
        @symbol.__send__(method, *args)
      end

      def class
        FieldSymbol
      end

      attr_writer :type, :desc
    end
  end
end
