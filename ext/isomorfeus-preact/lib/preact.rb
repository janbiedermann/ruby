if RUBY_ENGINE == 'opal'
  class VNode
    # just a empty place holder to make is_a?(VNode) work
    # internally using the js implementation
  end
end

class Fragment
  def initialize(props, _context)
    @props = props
  end

  def render
    @props[:children]
  end
end

module Preact
  EMPTY_ARR = []
  UNSAFE_NAME = /[\s\n\\\/='"\0<>]/

  if RUBY_ENGINE == 'opal'
    %x{
      function _catchError(error, vnode, oldVNode) {
        let component, ctor, handled;

        for (; (vnode = vnode._parent); ) {
          if ((component = vnode._component) && !component._processingException) {
            try {
              if (component["$respond_to?"]("get_derived_state_from_error")) {
                component.$set_state(component.$get_derived_state_from_error(error));
                handled = component._dirty;
              }

              if (component["$respond_to?"]("component_did_catch")) {
                component.$component_did_catch(error);
                handled = component._dirty;
              }

              // This is an error boundary. Mark it as having bailed out, and whether it was mid-hydration.
              if (handled) {
                return (component._pendingError = component);
              }
            } catch (e) {
              error = e;
            }
          }
        }

        throw error;
      }

      const EMPTY_OBJ = {};
      const EMPTY_ARR = [];
      const slice = EMPTY_ARR.slice;

      function assign(obj, props) {
        for (let i in props) obj[i] = props[i];
        return obj;
      }

      function applyRef(ref, value, vnode) {
        try {
          let converted_value;
          if (value == null || typeof(value) === 'undefined' ) { converted_value = nil; }
          else if (typeof value.$$class !== 'undefined') { converted_value = value; }
          else if (value instanceof Element || value instanceof Node) { converted_value = #{Browser::Element.new(`value`)}; }
          if (typeof ref === "function") ref.$call(converted_value);
          else ref["$[]="]("current", converted_value);
        } catch (e) {
          _catchError(e, vnode);
        }
      }

      function getDomSibling(vnode, childIndex) {
        if (childIndex == null) {
          // Use childIndex==null as a signal to resume the search from the vnode's sibling
          return vnode._parent
            ? getDomSibling(vnode._parent, vnode._parent._children.indexOf(vnode) + 1)
            : null;
        }

        let sibling;
        for (; childIndex < vnode._children.length; childIndex++) {
          sibling = vnode._children[childIndex];

          if (sibling != null && sibling._dom != null) {
            // Since updateParentDomPointers keeps _dom pointer correct,
            // we can rely on _dom to tell us if this subtree contains a
            // rendered DOM node, and what the first rendered DOM node is
            return sibling._dom;
          }
        }

        // If we get here, we have not found a DOM node in this vnode's children.
        // We must resume from this vnode's sibling (in it's parent _children array)
        // Only climb up and search the parent if we aren't searching through a DOM
        // VNode (meaning we reached the DOM parent of the original vnode that began
        // the search)
        return typeof vnode.type == 'function' ? getDomSibling(vnode) : null;
      }

      function placeChild(
        parentDom,
        childVNode,
        oldVNode,
        oldChildren,
        newDom,
        oldDom
      ) {
        let nextDom;
        if (childVNode._nextDom !== undefined) {
          // Only Fragments or components that return Fragment like VNodes will
          // have a non-undefined _nextDom. Continue the diff from the sibling
          // of last DOM child of this child VNode
          nextDom = childVNode._nextDom;

          // Eagerly cleanup _nextDom. We don't need to persist the value because
          // it is only used by `diffChildren` to determine where to resume the diff after
          // diffing Components and Fragments. Once we store it the nextDOM local var, we
          // can clean up the property
          childVNode._nextDom = undefined;
        } else if (oldVNode == null || oldVNode === nil || newDom != oldDom || newDom.parentNode == null || newDom.parentNode === nil) {
          outer: if (oldDom == null || oldDom === nil || oldDom.parentNode !== parentDom) {
            parentDom.appendChild(newDom);
            nextDom = null;
          } else {
            // `j<oldChildrenLength; j+=2` is an alternative to `j++<oldChildrenLength/2`
            for (
              let sibDom = oldDom, j = 0;
              (sibDom = sibDom.nextSibling) && j < oldChildren.length;
              j += 2
            ) {
              if (sibDom == newDom) {
                break outer;
              }
            }
            parentDom.insertBefore(newDom, oldDom);
            nextDom = oldDom;
          }
        }

        // If we have pre-calculated the nextDOM node, use it. Else calculate it now
        // Strictly check for `undefined` here cuz `null` is a valid value of `nextDom`.
        // See more detail in create-element.js:createVNode
        if (nextDom !== undefined) {
          oldDom = nextDom;
        } else {
          oldDom = newDom.nextSibling;
        }

        return oldDom;
      }

      function reorderChildren(childVNode, oldDom, parentDom) {
        // Note: VNodes in nested suspended trees may be missing _children.
        let c = childVNode._children;
        let tmp = 0;
        for (; c && tmp < c.length; tmp++) {
          let vnode = c[tmp];
          if (vnode) {
            // We typically enter this code path on sCU bailout, where we copy
            // oldVNode._children to newVNode._children. If that is the case, we need
            // to update the old children's _parent pointer to point to the newVNode
            // (childVNode here).
            vnode._parent = childVNode;

            if (typeof vnode.type == 'function') {
              oldDom = reorderChildren(vnode, oldDom, parentDom);
            } else {
              oldDom = placeChild(
                parentDom,
                vnode,
                vnode,
                c,
                vnode._dom,
                oldDom
              );
            }
          }
        }

        return oldDom;
      }

      function removeNode(node) {
        let parentNode = node.parentNode;
        if (parentNode) parentNode.removeChild(node);
      }

      self.unmount = function(vnode, parentVNode, skipRemove) {
        let r;

        if ((r = vnode.ref) && r && r !== nil) {
          try {
            if (typeof r === "function") {
              applyRef(r, null, parentVNode);
            } else {
              let rc = r["$[]"]("current");
              if (rc === nil || rc === vnode._dom) { applyRef(r, null, parentVNode); }
            }
          } catch (e) {
            // ignore error, continue unmount
          }
        }

        if ((r = vnode._component) != null && r !== nil) {
          if (r["$respond_to?"]("component_will_unmount")) {
            try {
              r.$component_will_unmount();
            } catch (e) {
              _catchError(e, parentVNode);
            }
          }

          r.base = r._parentDom = null;
        }

        if ((r = vnode._children)) {
          for (let i = 0; i < r.length; i++) {
            if (r[i]) {
              self.unmount(r[i], parentVNode, typeof vnode.type != 'function');
            }
          }
        }

        if (!skipRemove && vnode._dom != null && vnode._dom !== nil) removeNode(vnode._dom);

        // Must be set to `undefined` to properly clean up `_nextDom`
        // for which `null` is a valid value. See comment in `create-element.js`
        vnode._parent = vnode._dom = vnode._nextDom = undefined;
      }

      function diffChildren(
        parentDom,
        renderResult,
        newParentVNode,
        oldParentVNode,
        globalContext,
        isSvg,
        excessDomChildren,
        commitQueue,
        oldDom,
        isHydrating
      ) {
        let i, j, oldVNode, childVNode, newDom, firstChildDom, refs;

        // This is a compression of oldParentVNode!=null && oldParentVNode != EMPTY_OBJ && oldParentVNode._children || EMPTY_ARR
        // as EMPTY_OBJ._children should be `undefined`.
        let oldChildren = (oldParentVNode && oldParentVNode !== nil && oldParentVNode._children) ? oldParentVNode._children : EMPTY_ARR;

        let oldChildrenLength = oldChildren.length;

        newParentVNode._children = [];
        for (i = 0; i < renderResult.length; i++) {
          childVNode = renderResult[i];

          if (childVNode === nil || childVNode == null || typeof childVNode == 'boolean' || childVNode.$$is_boolean) {
            childVNode = newParentVNode._children[i] = null;
          }
          // If this newVNode is being reused (e.g. <div>{reuse}{reuse}</div>) in the same diff,
          // or we are rendering a component (e.g. setState) copy the oldVNodes so it can have
          // it's own DOM & etc. pointers
          else if (typeof childVNode == 'string' || typeof childVNode == 'number' || typeof childVNode == 'bigint') {
            childVNode = newParentVNode._children[i] = self.createVNode(
              null,
              childVNode,
              null,
              null,
              childVNode
            );
          } else if (childVNode.$$is_string || childVNode.$$is_number) {
            let str = childVNode.valueOf();
            childVNode = newParentVNode._children[i] = self.createVNode(
              null,
              str,
              null,
              null,
              str
            );
          } else if (Array.isArray(childVNode)) {
            childVNode = newParentVNode._children[i] = self.createVNode(
              Opal.Fragment,
              #{{ children: `childVNode` }},
              null,
              null,
              null
            );
          } else if (childVNode._depth > 0) {
            // VNode is already in use, clone it. This can happen in the following
            // scenario:
            //   const reuse = <div />
            //   <div>{reuse}<span />{reuse}</div>
            childVNode = newParentVNode._children[i] = self.createVNode(
              childVNode.type,
              childVNode.props,
              childVNode.key,
              childVNode.ref ? childVNode.ref : null,
              childVNode._original
            );
          } else {
            childVNode = newParentVNode._children[i] = childVNode;
          }

          if (childVNode === nil || childVNode == null) {
            continue;
          }
          childVNode._parent = newParentVNode;
          childVNode._depth = newParentVNode._depth + 1;

          // Check if we find a corresponding element in oldChildren.
          // If found, delete the array item by setting to `undefined`.
          // We use `undefined`, as `null` is reserved for empty placeholders
          // (holes).
          oldVNode = oldChildren[i];

          if (
            oldVNode === null || oldVNode === nil ||
            (oldVNode && oldVNode !== nil &&
              childVNode.key == oldVNode.key &&
              childVNode.type === oldVNode.type)
          ) {
            oldChildren[i] = undefined;
          } else {
            // Either oldVNode === undefined or oldChildrenLength > 0,
            // so after this loop oldVNode == null or oldVNode is a valid value.
            for (j = 0; j < oldChildrenLength; j++) {
              oldVNode = oldChildren[j];
              // If childVNode is unkeyed, we only match similarly unkeyed nodes, otherwise we match by key.
              // We always match by type (in either case).
              if (
                oldVNode && oldVNode !== nil &&
                childVNode.key == oldVNode.key &&
                childVNode.type === oldVNode.type
              ) {
                oldChildren[j] = undefined;
                break;
              }
              oldVNode = null;
            }
          }

          oldVNode = (oldVNode && oldVNode !== nil) ? oldVNode : EMPTY_OBJ;

          // Morph the old element into the new one, but don't append it to the dom yet
          diff(
            parentDom,
            childVNode,
            oldVNode,
            globalContext,
            isSvg,
            excessDomChildren,
            commitQueue,
            oldDom,
            isHydrating
          );

          newDom = childVNode._dom;

          if ((j = childVNode.ref) && j !== nil && oldVNode.ref != j) {
            if (!refs) refs = [];
            if (oldVNode.ref && oldVNode.ref !== nil) refs.push(oldVNode.ref, null, childVNode);
            refs.push(j, childVNode._component || newDom, childVNode);
          }

          if (newDom != null) {
            if (firstChildDom == null) {
              firstChildDom = newDom;
            }

            if (
              typeof childVNode.type == 'function' &&
              childVNode._children === oldVNode._children
            ) {
              childVNode._nextDom = oldDom = reorderChildren(
                childVNode,
                oldDom,
                parentDom
              );
            } else {
              oldDom = placeChild(
                parentDom,
                childVNode,
                oldVNode,
                oldChildren,
                newDom,
                oldDom
              );
            }

            if (typeof newParentVNode.type == 'function') {
              // Because the newParentVNode is Fragment-like, we need to set it's
              // _nextDom property to the nextSibling of its last child DOM node.
              //
              // `oldDom` contains the correct value here because if the last child
              // is a Fragment-like, then oldDom has already been set to that child's _nextDom.
              // If the last child is a DOM VNode, then oldDom will be set to that DOM
              // node's nextSibling.
              newParentVNode._nextDom = oldDom;
            }
          } else if (
            oldDom &&
            oldVNode._dom == oldDom &&
            oldDom.parentNode != parentDom
          ) {
            // The above condition is to handle null placeholders. See test in placeholder.test.js:
            // `efficiently replace null placeholders in parent rerenders`
            oldDom = getDomSibling(oldVNode);
          }
        }

        newParentVNode._dom = firstChildDom;

        // Remove remaining oldChildren if there are any.
        for (i = oldChildrenLength; i--; ) {
          if (oldChildren[i] != null) {
            if (
              typeof newParentVNode.type == 'function' &&
              oldChildren[i]._dom != null &&
              oldChildren[i]._dom == newParentVNode._nextDom
            ) {
              // If the newParentVNode.__nextDom points to a dom node that is about to
              // be unmounted, then get the next sibling of that vnode and set
              // _nextDom to it
              newParentVNode._nextDom = getDomSibling(oldParentVNode, i + 1);
            }

            self.unmount(oldChildren[i], oldChildren[i]);
          }
        }

        // Set refs only after unmount
        if (refs) {
          for (i = 0; i < refs.length; i++) {
            applyRef(refs[i], refs[++i], refs[++i]);
          }
        }
      }

      function eventProxy(e) {
        this._listeners[e.type + false].$call(#{Browser::Event.new(`e`)});
      }

      function eventProxyCapture(e) {
        this._listeners[e.type + true].$call(#{Browser::Event.new(`e`)});
      }

      const IS_NON_DIMENSIONAL = /acit|ex(?:s|g|n|p|$)|rph|grid|ows|mnc|ntw|ine[ch]|zoo|^ord|itera/i;

      function setStyle(style, key, value) {
        if (key[0] === '-') {
          style.setProperty(key, value);
        } else if (value == null || value === nil) {
          style[key] = '';
        } else if (typeof value != 'number' || IS_NON_DIMENSIONAL.test(key)) {
          style[key] = value;
        } else {
          style[key] = value + 'px';
        }
      }

      self.setProperty = function(dom, name, value, oldValue, isSvg) {
        let useCapture;

        o: if (name === 'style') {
          if (typeof value === 'string') {
            dom.style.cssText = value;
          } else {
            if (typeof oldValue === 'string') {
              dom.style.cssText = oldValue = '';
            }

            if (value && value !== nil && value.$$is_hash) {
              value = value.$to_n();
            }

            if (oldValue && oldValue !== nil && oldValue.$$is_hash) {
              oldValue = oldValue.$to_n();
            }

            if (oldValue && oldValue !== nil) {
              for (name in oldValue) {
                if (!(value && name in value)) {
                  setStyle(dom.style, name, '');
                }
              }
            }

            if (value && value !== nil) {
              for (name in value) {
                if (!oldValue || oldValue === nil || value[name] !== oldValue[name]) {
                  setStyle(dom.style, name, value[name]);
                }
              }
            }
          }
        }
        // Benchmark for comparison: https://esbench.com/bench/574c954bdb965b9a00965ac6
        else if (name[0] === 'o' && name[1] === 'n' && name[2] === '_') {
          useCapture = name !== (name = name.replace(/_capture$/, ''));

          // Infer correct casing for DOM built-in events:
          let namesl = name.slice(3);
          let domname = 'on' + namesl;
          if (domname in dom) name = namesl;
          else name = namesl;

          let evhandler = value;

          if (!dom._listeners) dom._listeners = {};
          dom._listeners[name + useCapture] = evhandler;

          if (value && value !== nil) {
            if (!oldValue || oldValue === nil) {
              const handler = useCapture ? eventProxyCapture : eventProxy;
              dom.addEventListener(name, handler, useCapture);
            }
          } else {
            const handler = useCapture ? eventProxyCapture : eventProxy;
            dom.removeEventListener(name, handler, useCapture);
          }
        } else if (name !== 'dangerouslySetInnerHTML') {
          if (isSvg) {
            // Normalize incorrect prop usage for SVG:
            // - xlink:href / xlinkHref --> href (xlink:href was removed from SVG and isn't needed)
            // - className --> class
            name = name.replace(/xlink(H|:h)/, 'h').replace(/sName$/, 's');
          } else if (
            name !== 'href' &&
            name !== 'list' &&
            name !== 'form' &&
            // Default value in browsers is `-1` and an empty string is
            // cast to `0` instead
            name !== 'tabIndex' &&
            name !== 'download' &&
            name in dom
          ) {
            try {
              dom[name] = (value == null || value === nil) ? '' : value;
              // labelled break is 1b smaller here than a return statement (sorry)
              break o;
            } catch (e) {}
          }

          // ARIA-attributes have a different notion of boolean values.
          // The value `false` is different from the attribute not
          // existing on the DOM, so we can't remove it. For non-boolean
          // ARIA-attributes we could treat false as a removal, but the
          // amount of exceptions would cost us too many bytes. On top of
          // that other VDOM frameworks also always stringify `false`.

          if (typeof value === 'function') {
            // never serialize functions as attribute values
          } else if (
            value != null && value !== nil &&
            (value !== false || (name[0] === 'a' && name[1] === 'r'))
          ) {
            dom.setAttribute(name, value);
          } else {
            dom.removeAttribute(name);
          }
        }
      }

      function diff_props(dom, new_props, old_props, is_svg, hydrate) {
        #{`old_props`.each do |prop, value|
            `if (prop !== "children" && prop !== "key" && !(prop.$$is_string && Object.hasOwnProperty.call(new_props.$$smap, prop))) { self.setProperty(dom, prop, null, value, is_svg); }`
            nil
          end
          `new_props`.each do |prop, value|
            if (`!hydrate || (prop[0] === 'o' && prop[1] === 'n' && prop[2] === '_')` || value.is_a?(Proc)) &&
              `prop !== "children" && prop !== "key" && prop !== "value" && prop !== "checked"` &&
              ((p = `old_props`[prop]) ? p : nil) != value
              `self.setProperty(dom, prop, value, old_props["$[]"](prop), is_svg)`
            end
          end
        }
      }

      function diffElementNodes(
        dom,
        newVNode,
        oldVNode,
        globalContext,
        isSvg,
        excessDomChildren,
        commitQueue,
        isHydrating
      ) {
        let oldProps = oldVNode.props;
        let newProps = newVNode.props;
        let nodeType = newVNode.type;
        let i = 0;

        // Tracks entering and exiting SVG namespace when descending through the tree.
        if (nodeType === 'svg') isSvg = true;

        if (excessDomChildren != null) {
          for (; i < excessDomChildren.length; i++) {
            const child = excessDomChildren[i];

            // if newVNode matches an element in excessDomChildren or the `dom`
            // argument matches an element in excessDomChildren, remove it from
            // excessDomChildren so it isn't later removed in diffChildren
            if (
              child &&
              'setAttribute' in child === !!nodeType &&
              (nodeType ? child.localName === nodeType : child.nodeType === 3)
            ) {
              dom = child;
              excessDomChildren[i] = null;
              break;
            }
          }
        }

        if (dom == null) {
          if (nodeType === null) {
            // createTextNode returns Text, we expect PreactElement
            return document.createTextNode(newProps);
          }

          if (isSvg) {
            dom = document.createElementNS(
              'http://www.w3.org/2000/svg',
              // We know `newVNode.type` is a string
              nodeType
            );
          } else {
            let np = newProps.$to_n();
            dom = document.createElement(
              // We know `newVNode.type` is a string
              nodeType,
              np.is && np
            );
          }

          // we created a new parent, so none of the previously attached children can be reused:
          excessDomChildren = null;
          // we are creating a new node, so we can assume this is a new subtree (in case we are hydrating), this deopts the hydrate
          isHydrating = false;
        }

        if (nodeType === null) {
          // During hydration, we still have to split merged text from SSR'd HTML.
          if (!oldProps || oldProps === nil || (!oldProps["$=="](newProps) && (!isHydrating || !dom.data["$=="](newProps)))) {
            dom.data = newProps;
          }
        } else {
          // If excessDomChildren was not null, repopulate it with the current element's children:
          excessDomChildren = excessDomChildren && slice.call(dom.childNodes);

          oldProps = oldVNode.props || Opal.hash();

          let oldHtml = oldProps["$[]"]("dangerouslySetInnerHTML");
          let newHtml = newProps["$[]"]("dangerouslySetInnerHTML");

          // During hydration, props are not diffed at all (including dangerouslySetInnerHTML)
          // @TODO we should warn in debug mode when props don't match here.
          if (!isHydrating) {
            // But, if we are in a situation where we are using existing DOM (e.g. replaceNode)
            // we should read the existing DOM attributes to diff them
            if (excessDomChildren != null) {
              oldProps = Opal.hash();
              for (i = 0; i < dom.attributes.length; i++) {
                oldProps["$[]="](dom.attributes[i].name, dom.attributes[i].value);
              }
            }

            if (newHtml !== nil || oldHtml !== nil) {
              // Avoid re-applying the same '__html' if it has not changed between re-render
              if (
                newHtml === nil ||
                ((oldHtml === nil || newHtml["$[]"]("__html") != oldHtml["$[]"]("__html")) &&
                  newHtml["$[]"]("__html") !== dom.innerHTML)
              ) {
                dom.innerHTML = (newHtml !== nil && newHtml["$[]"]("__html")) || '';
              }
            }
          }

          diff_props(dom, newProps, oldProps, isSvg, isHydrating);

          // If the new vnode didn't have dangerouslySetInnerHTML, diff its children
          if (newHtml !== nil) {
            newVNode._children = [];
          } else {
            i = newVNode.props["$[]"]("children");
            diffChildren(
              dom,
              Array.isArray(i) ? i : [i],
              newVNode,
              oldVNode,
              globalContext,
              isSvg && nodeType !== 'foreignObject',
              excessDomChildren,
              commitQueue,
              excessDomChildren
                ? excessDomChildren[0]
                : oldVNode._children && getDomSibling(oldVNode, 0),
              isHydrating
            );

            // Remove children that are not part of any vnode.
            if (excessDomChildren != null) {
              for (i = excessDomChildren.length; i--; ) {
                if (excessDomChildren[i] != null) removeNode(excessDomChildren[i]);
              }
            }
          }

          // (as above, don't diff props during hydration)
          if (!isHydrating) {
            if (
              // instead of newProps["$key?"]("value")
              Object.hasOwnProperty.call(newProps.$$smap, "value") &&
              (i = newProps["$[]"]("value")) !== nil &&
              // #2756 For the <progress>-element the initial value is 0,
              // despite the attribute not being present. When the attribute
              // is missing the progress bar is treated as indeterminate.
              // To fix that we'll always update it when it is 0 for progress elements
              (i !== dom.value || (nodeType === 'progress' && !i) ||
                // This is only for IE 11 to fix <select> value not being updated.
                // To avoid a stale select value we need to set the option.value
                // again, which triggers IE11 to re-evaluate the select value
                (nodeType === 'option' && i !== oldProps["$[]"]("value")))
            ) {
              self.setProperty(dom, 'value', i, oldProps["$[]"]("value"), false, null);
            }
            if (
              Object.hasOwnProperty.call(newProps.$$smap, "checked") &&
              (i = newProps["$[]"]("checked")) !== nil &&
              i !== dom.checked
            ) {
              self.setProperty(dom, 'checked', i, oldProps["$[]"]("checked"), false, null);
            }
          }
        }

        return dom;
      }

      function validate_props(newType, newProps) {
        if (newType.declared_props && newType.declared_props !== nil) {
          #{
            `newType.declared_props`.each do |prop, value|
              `if (Object.hasOwnProperty.call(value.$$smap, "default") && !Object.hasOwnProperty.call(newProps.$$smap, prop)) { #{`newProps`[prop] = value[:default]} }`
              nil
            end
          }
          if (Opal.Isomorfeus.development) { #{`newType`.validate_props(`newProps`)} }
        }
      }

      function diff(
        parentDom,
        newVNode,
        oldVNode,
        globalContext,
        isSvg,
        excessDomChildren,
        commitQueue,
        oldDom,
        isHydrating
      ) {
        let newType = newVNode.type;

        // When passing through createElement it assigns the object
        // constructor as undefined. This to prevent JSON-injection.
        if (newVNode.constructor !== undefined) return null;

        // If the previous diff bailed out, resume creating/hydrating.
        if (oldVNode._hydrating != null) {
          isHydrating = oldVNode._hydrating;
          oldDom = newVNode._dom = oldVNode._dom;
          // if we resume, we want the tree to be "unlocked"
          newVNode._hydrating = null;
          excessDomChildren = [oldDom];
        }

        try {
          outer: if (typeof newType == 'function') {
            let c, ctxType, isNew, oldProps, oldState, renderResult, snapshot, clearProcessingException;
            let newProps = newVNode.props;

            // Necessary for createContext api. Setting this property will pass
            // the context value as `this.context` just for this component.
            ctxType = newType.context_type;
            let provider = (ctxType && ctxType !== nil) && globalContext["$[]"](ctxType.context_id);
            let componentContext = (ctxType && ctxType !== nil) ? ((provider && provider !== nil) ? provider.props["$[]"]("value") : ctxType.value) : globalContext;

            // Get component and set it to `c`
            if (oldVNode._component) {
              c = newVNode._component = oldVNode._component;
              clearProcessingException = c._processingException = c._pendingError;
            } else {
              // Instantiate the new component
              // validate props
              validate_props(newType, newProps);
              newProps.$freeze();
              // The check above verifies that newType is supposed to be constructed
              newVNode._component = c = newType.$new(newProps, componentContext);

              if (provider && provider !== nil) provider.$sub(c);

              c.props = newProps;
              if (c.state === nil || !c.state) c.state = Opal.hash();
              c.context = componentContext;
              c._globalContext = globalContext;
              isNew = c._dirty = true;
              c._renderCallbacks = [];
            }

            // Invoke get_derived_state_from_props
            if (!c._nextState || c._nextState === nil) {
              c._nextState = c.state;
            }

            if (!isNew) { validate_props(newType, newProps); }
            newProps.$freeze();
            if (c["$respond_to?"]("get_derived_state_from_props")) {
              if (c._nextState == c.state) {
                c._nextState = c._nextState.$dup();
              }
              c._nextState["$merge!"](c.$get_derived_state_from_props(newProps, c._nextState));
            }

            oldProps = c.props;
            oldState = c.state;
            c._nextState.$freeze();

            // Invoke pre-render lifecycle methods
            if (isNew) {
              if (c["$respond_to?"]("component_did_mount")) {
                c._renderCallbacks.push(c.$component_did_mount);
              }
            } else {
              if (
                (!c._force &&
                  c["$respond_to?"]("should_component_update?") &&
                  c["$should_component_update?"](
                    newProps,
                    c._nextState,
                    componentContext
                  ) === false) ||
                newVNode._original === oldVNode._original
              ) {
                c.props = newProps;
                c.state = c._nextState;
                // More info about this here: https://gist.github.com/JoviDeCroock/bec5f2ce93544d2e6070ef8e0036e4e8
                if (newVNode._original !== oldVNode._original) c._dirty = false;
                c._vnode = newVNode;
                newVNode._dom = oldVNode._dom;
                newVNode._children = oldVNode._children;
                newVNode._children.forEach(vnode => {
                  if (vnode) vnode._parent = newVNode;
                });
                if (c._renderCallbacks.length) {
                  commitQueue.push(c);
                }

                break outer;
              }

              if (c["$respond_to?"]("component_did_update")) {
                c._renderCallbacks.push(() => {
                  c.$component_did_update(oldProps, oldState, snapshot);
                });
              }
            }

            c.context = componentContext;
            c.props = newProps;
            c._vnode = newVNode;
            c._parentDom = parentDom;
            c.state = c._nextState;
            c._dirty = false;

            renderResult = c.$render();

            // Handle setState called in render, see #2553
            c.state = c._nextState;

            if (c.getChildContext != null) {
              globalContext = globalContext.$merge(c.$get_child_context());
            }

            if (!isNew && c["$respond_to?"]("get_snapshot_before_update")) {
              snapshot = c.$get_snapshot_before_update(oldProps, oldState);
            }

            if (renderResult !== nil && renderResult != null && renderResult.type === Opal.Fragment && renderResult.key == null) {
              renderResult = renderResult.props["$[]"]("children");
            }

            diffChildren(
              parentDom,
              Array.isArray(renderResult) ? renderResult : [renderResult],
              newVNode,
              oldVNode,
              globalContext,
              isSvg,
              excessDomChildren,
              commitQueue,
              oldDom,
              isHydrating
            );

            c.base = newVNode._dom;

            // We successfully rendered this VNode, unset any stored hydration/bailout state:
            newVNode._hydrating = null;

            if (c._renderCallbacks.length) {
              commitQueue.push(c);
            }

            if (clearProcessingException) {
              c._pendingError = c._processingException = null;
            }

            c._force = false;

            } else if (
            excessDomChildren == null &&
            newVNode._original === oldVNode._original
          ) {
            newVNode._children = oldVNode._children;
            newVNode._dom = oldVNode._dom;
          } else {
            newVNode._dom = diffElementNodes(
              oldVNode._dom,
              newVNode,
              oldVNode,
              globalContext,
              isSvg,
              excessDomChildren,
              commitQueue,
              isHydrating
            );
          }
        } catch (e) {
          newVNode._original = null;
          // if hydrating or creating initial tree, bailout preserves DOM:
          if (isHydrating || excessDomChildren != null) {
            newVNode._dom = oldDom;
            newVNode._hydrating = !!isHydrating;
            excessDomChildren[excessDomChildren.indexOf(oldDom)] = null;
            // ^ could possibly be simplified to:
            // excessDomChildren.length = 0;
          }
          _catchError(e, newVNode, oldVNode);
        }
      }

      function commitRoot(commitQueue, root) {
        commitQueue.some(c => {
          try {
            // Reuse the commitQueue variable here so the type changes
            commitQueue = c._renderCallbacks;
            c._renderCallbacks = [];
            commitQueue.some(cb => {
              // See above comment on commitQueue
              cb.call(c);
            });
          } catch (e) {
            _catchError(e, c._vnode);
          }
        });
      }

      let vnodeId = 0;
      const vnode_class = #{VNode};

      function is_a_vnode(type) { return type === vnode_class; }
      function is_nil() { return false; }
      function vnode_eql(me, other) {
        for(let prop in me) {
          if (prop === 'props') { continue; }
          else if (me[prop] != other[prop]) { return false; }
        }
        return me.props["$=="](other.props);
      }

      self.createVNode = function(type, props, key, ref, original) {
        // V8 seems to be better at detecting type shapes if the object is allocated from the same call site
        // Do not inline into createElement and coerceToVNode!
        let eql;
        const vnode = {
          type,
          props,
          key,
          ref,
          _children: null,
          _parent: null,
          _depth: 0,
          _dom: null,
          // _nextDom must be initialized to undefined b/c it will eventually
          // be set to dom.nextSibling which can return `null` and it is important
          // to be able to distinguish between an uninitialized _nextDom and
          // a _nextDom that has been set to `null`
          _nextDom: undefined,
          _component: null,
          _hydrating: null,
          constructor: undefined,
          _original: (original == null) ? ++vnodeId : original,
          "$is_a?": is_a_vnode,
          "$==": eql = function(other) { return vnode_eql(vnode, other); },
          "$eql?": eql,
          "$nil?": is_nil,
          "$$is_vnode": true
        };
        return vnode;
      }

      self.render = function(vnode, parentDom, replaceNode) {
        // We abuse the `replaceNode` parameter in `hydrate()` to signal if we are in
        // hydration mode or not by passing the `hydrate` function instead of a DOM
        // element..
        let isHydrating = typeof replaceNode === 'function';

        let reno = (replaceNode !== nil && replaceNode);
        let nohy_reno = (!isHydrating && reno);

        // To be able to support calling `render()` multiple times on the same
        // DOM node, we need to obtain a reference to the previous tree. We do
        // this by assigning a new `_children` property to DOM nodes which points
        // to the last rendered tree. By default this property is not present, which
        // means that we are mounting a new tree for the first time.
        let oldVNode = isHydrating
          ? null
          : (reno && replaceNode._children) || parentDom._children;

        let ov = (oldVNode && oldVNode !== nil);

        vnode = (
          nohy_reno || parentDom
        )._children = self.$create_element(Opal.Fragment, nil, [vnode]);

        // List of effects that need to be called after diffing.
        let commitQueue = [];
        diff(
          parentDom,
          // Determine the new vnode tree and store it on the DOM element on
          // our custom `_children` property.
          vnode,
          ov ? oldVNode : EMPTY_OBJ,
          Opal.hash(),
          parentDom.ownerSVGElement !== undefined,
          nohy_reno ? [replaceNode] : ov ? null : parentDom.firstChild ? slice.call(parentDom.childNodes) : null,
          commitQueue,
          nohy_reno ? replaceNode : ov ? oldVNode._dom : parentDom.firstChild,
          isHydrating
        );

        // Flush all queued effects
        commitRoot(commitQueue, vnode);
      };

      function updateParentDomPointers(vnode) {
        if ((vnode = vnode._parent) != null && vnode._component != null) {
          vnode._dom = vnode._component.base = null;
          for (let i = 0; i < vnode._children.length; i++) {
            let child = vnode._children[i];
            if (child != null && child._dom != null) {
              vnode._dom = vnode._component.base = child._dom;
              break;
            }
          }

          return updateParentDomPointers(vnode);
        }
      }

      function renderComponent(component) {
        let vnode = component._vnode,
          oldDom = vnode._dom,
          parentDom = component._parentDom;

        if (parentDom) {
          let commitQueue = [];
          const oldVNode = assign({}, vnode);
          oldVNode._original = vnode._original + 1;

          diff(
            parentDom,
            vnode,
            oldVNode,
            component._globalContext,
            parentDom.ownerSVGElement !== undefined,
            vnode._hydrating != null ? [oldDom] : null,
            commitQueue,
            (oldDom == null || oldDom === nil) ? getDomSibling(vnode) : oldDom,
            vnode._hydrating
          );
          commitRoot(commitQueue, vnode);

          if (vnode._dom != oldDom) {
            updateParentDomPointers(vnode);
          }
        }
      }

      self.process = function() {
        let queue;
        while ((self.process._rerenderCount = self.rerender_queue.length)) {
          queue = self.rerender_queue.sort((a, b) => a._vnode._depth - b._vnode._depth);
          self.rerender_queue = [];
          // Don't update `renderCount` yet. Keep its value non-zero to prevent unnecessary
          // process() calls from getting scheduled while `queue` is still being consumed.
          queue.some(c => {
            if (c._dirty) renderComponent(c);
          });
        }
      }
      self.process._rerenderCount = 0;
    }
  else
    IS_NON_DIMENSIONAL = /acit|ex(?:s|g|n|p|$)|rph|grid|ows|mnc|ntw|ine[ch]|zoo|^ord|^--/i
    JS_TO_CSS = {}
    ENCODED_ENTITIES = /[&<>"]/
  end

  class << self
    def _ctxi
      @_ctxi ||= 0
    end

    def _ctxi=(i)
      @_ctxi = i
    end

    def _context_id
      "__cC#{self._ctxi += 1}"
    end

    def clone_element(vnode, props = nil, children = nil)
      normalized_props = {}
      if RUBY_ENGINE == 'opal'
        normalized_props.merge!(`vnode.props`)
      else
        normalized_props.merge!(vnode.props)
      end

      if props
        normalized_props.merge!(props)
        key = normalized_props.delete(:key)
        ref = normalized_props.delete(:ref)
      else
        key = nil
        ref = nil
      end

      normalized_props[:children] = children unless children.nil?

      if RUBY_ENGINE == 'opal'
        `self.createVNode(vnode.type, normalized_props, #{key || `vnode.key`}, #{ref || `vnode.ref`}, null)`
      else
        VNode.new(vnode.type, normalized_props, key || vnode.key, ref || vnode.ref)
      end
    end

    def create_context(const_name, default_value = nil)
      context = Preact::Context.new(default_value)
      Object.const_set(const_name, context)
    end

    def _init_render
      self.render_buffer = []
      self.rerender_queue = []
      Isomorfeus.reset_something_loading
    end

    if RUBY_ENGINE == 'opal'
      attr_accessor :_vnode_id
      attr_accessor :render_buffer
      attr_accessor :rerender_queue

      def is_renderable?(block_result)
        block_result &&
          (block_result.JS['$$is_vnode'] || block_result.JS['$$is_string'] || block_result.is_a?(Numeric) ||
            (block_result.JS['$$is_array'] && `block_result.length > 0` && is_renderable?(block_result[0])))
      end

      def create_element(type, props = nil, children = nil)
        if props
          if props.JS['$$is_hash']
            normalized_props = props.dup
            key = normalized_props.delete(:key)
            ref = normalized_props.delete(:ref)
          else
            children = props
            normalized_props = {}
            key = nil
            ref = nil
          end
        else
          normalized_props = {}
          key = nil
          ref = nil
        end

        if block_given?
          pr = `self.render_buffer`
          pr.JS.push([])
          block_result = yield
          c = pr.JS.pop()
          %x{
            if (self["$is_renderable?"](block_result)) { c.push(block_result); }
            if (c.length > 0) { children = c; }
          }
        end

        %x{
          if (children !== nil && children !== null) { normalized_props["$[]="]("children", children); }
          return self.createVNode(type, normalized_props, key, ref, null);
        }
      end

      def _enqueue_render(c)
        if ((`!c._dirty` && (`c._dirty = true`) && (rerender_queue << c) && `!self.process._rerenderCount++`))
          `setTimeout(self.process)`
        end
      end

      def _render_element(component, props, &block)
        %x{
          let opr = Opal.Preact.render_buffer;
          opr[opr.length-1].push(#{create_element(component, props, nil, &block)});
        }
        nil
      end

      def hydrate(vnode, container_node)
        render(vnode, container_node, `self.render`)
      end

      def element_or_query_to_n(element_or_query)
        if `!element_or_query || element_or_query === nil`
          return `null`
        elsif `(element_or_query instanceof HTMLElement)`
          return element_or_query
        elsif `(typeof element_or_query === 'string')` || element_or_query.is_a?(String)
          return `document.body.querySelector(element_or_query)`
        elsif `(typeof element_or_query === 'function')`
          return element_or_query
        elsif element_or_query.is_a?(Browser::Element)
          return element_or_query.to_n
        else
          return element_or_query
        end
      end

      def render(vnode, container_node, replace_node = nil)
        _init_render
        container_node = element_or_query_to_n(container_node)
        replace_node = element_or_query_to_n(replace_node)
        `self.render(vnode, container_node, replace_node)`
      end

      def unmount_component_at_node(element_or_query)
        element_or_query = element_or_query_to_n(element_or_query)
        `self.render(null, element_or_query)`
      end
    else # RUBY_ENGINE
      def render_buffer
        Thread.current[:@_isomorfeus_preact_render_buffer]
      end

      def render_buffer=(i)
        Thread.current[:@_isomorfeus_preact_render_buffer] = i
      end

      def rerender_queue
        Thread.current[:@_isomorfeus_preact_rerender_queue]
      end

      def rerender_queue=(i)
        Thread.current[:@_isomorfeus_preact_rerender_queue] = i
      end

      def is_renderable?(block_result)
        block_result &&
          (block_result.is_a?(VNode) || block_result.is_a?(String) || block_result.is_a?(Numeric) ||
            (block_result.is_a?(Array) && block_result.length > 0 && is_renderable?(block_result[0])))
      end

      def _encode_entities(input)
        s = input.to_s
        return s unless ENCODED_ENTITIES.match?(s)
        # TODO performance maybe, maybe similar to new js way, need to measure
        # for (; i<str.length; i++) {
        #   switch (str.charCodeAt(i)) {
        #     case 60: ch = '&lt;'; break;
        #     case 62: ch = '&gt;'; break;
        #     case 34: ch = '&quot;'; break;
        #     case 38: ch = '&amp;'; break;
        #     default: continue;
        #   }
        #   if (i > start) out += str.slice(start, i);
        #   out += ch;
        #   start = i + 1;
        # }
        s.gsub(/&/, '&amp;').gsub(/</, '&lt;').gsub(/>/, '&gt;').gsub(/"/, '&quot;')
      end

      def _style_obj_to_css(v)
        str = ''
        v.each do |prop, val|
          if val != nil && val != ''
            str << ' ' if !str.empty?
            prop_s = prop.to_s
            str << prop_s[0] == '-' ? prop_s : JS_TO_CSS[prop] || (JS_TO_CSS[prop] = prop.gsub(/([A-Z])/, "-\\1").downcase)
            str << ': '
            str << "#{val}"
            str << 'px' if val.is_a?(Numeric) && IS_NON_DIMENSIONAL.match?(prop_s) == false
            str << ';'
          end
        end
        return str.empty? ? nil : str
      end

      def _render_element(element, props, &block)
        pr = Preact.render_buffer
        pr[pr.length-1] << create_element(element, props, nil, &block)
        nil
      end

      def render_to_string(vnode, context = nil)
        _init_render
        context = {} unless context
        _render_to_string(vnode, context, false, nil)
      end
    end # RUBY_ENGINE
  end
end

if RUBY_ENGINE == 'opal'
  Preact._vnode_id = 0
end
