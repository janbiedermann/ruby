module Preact::MathMlElements
  # https://www.w3.org/TR/MathML3/appendixi.html#index.elem
  SUPPORTED_MATH_ML_ELEMENTS = %w[
    abs and annotation apply approx arccos arccosh arccot arccoth arccsc arccsch
    arcsec arcsech arcsin arcsinh arctan arctanh arg
    bind bvar
    card cartesianproduct cbytes ceiling cerror ci cn codomain complexes compose
    condition conjugate cos cosh cot coth cs csc csch csymbol curl
    declare degree determinant diff divergence divide domain domainofapplication
    emptyset eq equivalent eulergamma exists exp exponentiale
    factorial factorof false floor fn forall
    gcd geq grad gt
    ident image imaginary imaginaryi implies in infinity int integers intersect
    interval inverse lambda laplacian lcm leq limit list ln log logbase
    lowlimit lt
    maction maligngroup malignmark matrix matrixrow max mean median
    menclose merror mfenced mfrac mglyph mi min minus mlabeledtr mlongdiv
    mmultiscripts mn mo mode moment momentabout mover mpadded mphantom
    mprescripts mroot mrow ms mscarries mscarry msgroup msline mspace msqrt
    msrow mstack mstyle msub msubsup msup mtable mtd mtext mtr munder
    munderover
    naturalnumbers neq none not notanumber notin notprsubset notsubset
    or otherwise outerproduct
    partialdiff pi piece piecewise plus power primes product prsubset
    quotient
    rationals real reals reln rem root
    scalarproduct sdev sec sech selector semantics sep set setdiff share sin
    sinh subset sum
    tan tanh tendsto times transpose true
    union uplimit
    variance vector vectorproduct
    xor
  ]

  if RUBY_ENGINE == 'opal'
    SUPPORTED_MATH_ML_ELEMENTS.each do |element|
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
    SUPPORTED_MATH_ML_ELEMENTS.each do |element|
      define_method(element.to_s.underscore.upcase.to_sym) do |props = nil, &block|
        Preact._render_element(element, props, &block)
      end
    end
  end
end
