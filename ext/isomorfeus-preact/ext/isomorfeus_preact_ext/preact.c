#include <ruby.h>
#include <ruby/re.h>
#undef DEPRECATED
#define FIO_STR_NAME render_result
#include "fio-stl.h"
#include "isomorfeus_preact_ext.h"

static ID id_checked;
static ID id_children;
static ID id_class;
static ID id_class_name;
static ID id_class_name2;
static ID id_context_id;
static ID id_context_type;
static ID id_dangerously_set_inner_html;
static ID id_default_checked;
static ID id_default_selected;
static ID id_default_value;
static ID id_development;
static ID id_downcase;
static ID id_encode_entities;
static ID id_eqeq;
static ID id_for;
static ID id_freeze;
static ID id_get_child_context;
static ID id_get_derived_state_from_props;
static ID id_gsub;
static ID id_html_for;
static ID id_include;
static ID id_is_renderable;
static ID id_key;
static ID id_keyq;
static ID id_keys;
static ID id_merge;
static ID id_new;
static ID id_props;
static ID id_ref;
static ID id_render;
static ID id_render_buffer;
static ID id_selected;
static ID id_self;
static ID id_source;
static ID id_start_withq;
static ID id_style;
static ID id_style_obj_to_css;
static ID id_validate_props;
static ID id_value;

static ID iv_context;
static ID iv_declared_props;
static ID iv_next_state;
static ID iv_props;
static ID iv_state;

static char *str_foreign_object = "foreignObject";
static char *str_option = "option";
static char *str_select = "select";
static char *str_selected = "selected";
static char *str_svg = "svg";
static char *str_textarea = "textarea";

static char *str_rve = "^(area|base|br|col|embed|hr|img|input|link|meta|param|source|track|wbr)$";
static char *str_run = "[\\s\\n\\\\\\/='\"\\0<>]";
static char *str_rxl = "^xlink:?.";
static char *str_rx2 = "^xlink:?";
// static char *str_ree = "[\"&<]";

static VALUE s_aria;
static VALUE s_false;
static VALUE s_true;
static VALUE s_xlink;

static VALUE sym_children;
static VALUE sym_class;
static VALUE sym_default;
static VALUE sym_for;
static VALUE sym_html;
static VALUE sym_key;
static VALUE sym_ref;
static VALUE sym_selected;
static VALUE sym_value;

static VALUE mIsomorfeus;
static VALUE mPreact;
static VALUE cFragment;

extern VALUE cVNode;

static VALUE rXlink;
static VALUE rUnsafe;
static VALUE rVoid;
static VALUE rXlink2;

int set_default_prop_value(VALUE prop, VALUE value, VALUE props) {
  if ((rb_funcall(value, id_keyq, 1, sym_default) == Qtrue) && (rb_funcall(props, id_keyq, 1, prop) == Qfalse)) {
    rb_hash_aset(props, props, rb_hash_lookup(value, sym_default));
  }
  return 0;
}

ID normalize_prop_name(ID name_id, VALUE is_svg_mode) {
  if (name_id == id_class_name || name_id == id_class_name2) { return id_class; }
  if (name_id == id_html_for) { return id_for; }
  if (name_id == id_default_value) { return id_value; }
  if (name_id == id_default_checked) { return id_checked; }
  if (name_id == id_default_selected) { return id_selected; }
  if (is_svg_mode == Qtrue) {
    VALUE name_s = rb_id2str(name_id);
    if (rb_reg_match(rXlink, name_s) == Qtrue) {
      name_s = rb_funcall(name_s, id_downcase, 0);
      name_s = rb_funcall(name_s, id_gsub, 2, rXlink2, s_xlink);
      return rb_to_id(name_s);
    }
  }
  return name_id;
}

VALUE normalize_prop_value(ID name_id, VALUE name_s, VALUE value, VALUE self) {
	if (name_id == id_style && value != Qnil && TYPE(value) == T_HASH) {
		return rb_funcall(self, id_style_obj_to_css, 1, value);
	}
  // name[0] === 'a' && name[1] === 'r'
  if (value == Qtrue && rb_funcall(name_s, id_start_withq, 1, s_aria) == Qtrue) {
    // always use string values instead of booleans for aria attributes
		// also see https://github.com/preactjs/preact/pull/2347/files
    return s_true;
	}
  if (value == Qfalse && rb_funcall(name_s, id_start_withq, 1, s_aria) == Qtrue) {
    // always use string values instead of booleans for aria attributes
		// also see https://github.com/preactjs/preact/pull/2347/files
    return s_false;
	}
	return value;
}

VALUE get_context(VALUE node_type, VALUE context) {
  VALUE ctx_type = rb_funcall(node_type, id_context_type, 0);
  VALUE provider = Qnil;
  VALUE cctx = context;
  if (ctx_type != Qnil) {
    provider = rb_hash_lookup(context, rb_funcall(ctx_type, id_context_id, 0));
    if (provider != Qnil) {
      cctx = rb_hash_lookup(rb_funcall(provider, id_props, 0), sym_value); }
    else {
      cctx = rb_funcall(ctx_type, id_value, 0); }
  }
  return cctx;
}

VALUE render_class_component(VALUE vnode, VALUE context) {
  VNode *v = (VNode *)DATA_PTR(vnode);

	VALUE node_type = v->type;
  VALUE props = v->props;

  // get context
	VALUE cctx = get_context(node_type, context);

  // validate props
  VALUE declared_props = rb_ivar_get(node_type, iv_declared_props);
  if (declared_props != Qnil) {
    rb_hash_foreach(declared_props, set_default_prop_value, props);
    if (rb_funcall(mIsomorfeus, id_development, 0) == Qtrue) { rb_funcall(node_type, id_validate_props, 1, props); }
  }

  // freeze props
  rb_obj_freeze(props);

  // instantiate component
  VALUE c = rb_funcall(node_type, id_new, 2, props, cctx);
	v->component = c;

  // set state, props, context
  rb_ivar_set(c, iv_props, props);
  if (rb_ivar_get(c, iv_state) == Qnil)
    rb_ivar_set(c, iv_state, rb_hash_new());
  if (rb_ivar_get(c, iv_next_state) == Qnil)
    rb_ivar_set(c, iv_next_state, rb_ivar_get(c, iv_state));
  rb_ivar_set(c, iv_context, cctx);

  // get_derived_state_from_props
  if (rb_respond_to(c, id_get_derived_state_from_props)) {
    VALUE state = rb_ivar_get(c, iv_state);
    VALUE res = rb_funcall(c, id_get_derived_state_from_props, 2, rb_funcall(c, id_props, 0), state);
    rb_ivar_set(c, iv_state, rb_funcall(state, id_merge, res));
  }

  // freeze state
  rb_obj_freeze(rb_ivar_get(c, iv_state));

  // render
	return rb_funcall(c, id_render, 0);
}

void internal_render_to_string(VALUE vnode, VALUE context, VALUE is_svg_mode, VALUE select_value, VALUE self, render_result_s *rres) {
  VNode *v;
  VALUE res;
  VALUE node_type, props;
  int i, len;

  if (vnode == Qnil || vnode == Qfalse || vnode == Qtrue) {
    return;
  }

  switch (TYPE(vnode)) {
    case T_STRING:
      if (RSTRING_LEN(vnode) == 0) { return; }
      res = rb_funcall(self, id_encode_entities, 1, vnode);
      render_result_write(rres, RSTRING_PTR(res), RSTRING_LEN(res));
      return;
    case T_FIXNUM:
    case T_BIGNUM:
      res = rb_obj_as_string(vnode);
      render_result_write(rres, RSTRING_PTR(res), RSTRING_LEN(res));
      return;
    case T_ARRAY:
      len = RARRAY_LEN(vnode);
      for (i = 0; i < len; i++) {
        internal_render_to_string(RARRAY_PTR(vnode)[i], context, is_svg_mode, select_value, self, rres);
      }
      return;
  }

  v = (VNode *)DATA_PTR(vnode);
  node_type = v->type;
  props = v->props;

  if (TYPE(node_type) != T_STRING) {
    // components and Fragments
    VALUE rendered;

    if (node_type == cFragment) {
      rendered = rb_hash_lookup(props, sym_children);
    } else {
      rendered = render_class_component(vnode, context);

      VALUE component = v->component;
      if (rb_respond_to(component, id_get_child_context)) {
        context = rb_funcall(context, id_merge, 1, rb_funcall(component, id_get_child_context, 0));
      }
    }

    internal_render_to_string(rendered, context, is_svg_mode, select_value, self, rres);
    return;
  }

  props = v->props;

  // render JSX to HTML
  VALUE node_type_s = rb_obj_as_string(node_type);
  render_result_write(rres, "<", 1);
  render_result_write(rres, RSTRING_PTR(node_type_s), RSTRING_LEN(node_type_s));

  VALUE children = Qnil;
  VALUE html = Qnil;

  if (props != Qnil) {
    children = rb_hash_lookup(props, sym_children);
    VALUE attrs = rb_funcall(props, id_keys, 0);
    len = RARRAY_LEN(attrs);
    for (i = 0; i < len; i++) {
      VALUE name = RARRAY_PTR(attrs)[i];
      ID name_id = rb_to_id(name);
      VALUE value = rb_hash_lookup(props, rb_to_symbol(name));

      if (name_id == id_key || name_id == id_ref || name_id == id_self || name_id == id_source || name_id == id_children ||
        ((name_id == id_class_name || name_id == id_class_name2) && (rb_hash_lookup(props, sym_class) != Qnil)) ||
        ((name_id == id_html_for) && (rb_hash_lookup(props, sym_for) != Qnil))) {
        continue;
      }

      VALUE orig_name_id = name_id;
      VALUE name_s = rb_id2str(name_id);
      if (rb_reg_match(rUnsafe, name_s) == Qtrue) {
        continue;
      }

      name_id = normalize_prop_name(name_id, is_svg_mode);
      if (name_id != orig_name_id)
        name_s = rb_id2str(name_id);
      value = normalize_prop_value(name_id, name_s, value, self);

      if (name_id == id_dangerously_set_inner_html && value != Qnil) {
        html = rb_hash_lookup(value, sym_html);
      } else if (name_id == id_value && strcmp(RSTRING_PTR(node_type_s), str_textarea) == 0) {
        // <textarea value="a&b"> --> <textarea>a&amp;b</textarea>
        children = value;
      } else if (value != Qnil && value != Qfalse && rb_obj_is_proc(value) != Qtrue) {
        if (value == Qtrue || ((TYPE(value) == T_STRING) && (RSTRING_PTR(value)[0] == 0))) {
          // if (name_id == id_class || name_id == id_style) { continue; }
          value = name_s;
          render_result_write(rres, " ", 1);
          render_result_write(rres, RSTRING_PTR(name_s), RSTRING_LEN(name_s));
          continue;
        }

        if (name_id == id_value) {
          if (strcmp(RSTRING_PTR(node_type_s), str_select) == 0) {
            select_value = value;
            continue;
          } else if (
            // If we're looking at an <option> and it's the currently selected one
            strcmp(RSTRING_PTR(node_type_s), str_option) == 0 &&
            rb_funcall(select_value, id_eqeq, 1, value) == Qtrue &&
            // and the <option> doesn't already have a selected attribute on it
            rb_hash_lookup(props, sym_selected) == Qnil
          ) {
            render_result_write(rres, " ", 1);
            render_result_write(rres, str_selected, strlen(str_selected));
          }
        }

        render_result_write(rres, " ", 1);
        render_result_write(rres, RSTRING_PTR(name_s), RSTRING_LEN(name_s));
        render_result_write(rres, "=\"", 2);
        res = rb_funcall(self, id_encode_entities, 1, value);
        render_result_write(rres, RSTRING_PTR(res), RSTRING_LEN(res));
        render_result_write(rres, "\"", 1);
      }
    }
  }

  render_result_write(rres, ">", 1);

  if (rb_reg_match(rUnsafe, node_type_s) == Qtrue) {
    rb_raise(rb_eRuntimeError, "%s is not a valid HTML tag name in %s", RSTRING_PTR(node_type_s), render_result_ptr(rres));
  }

  render_result_s *pieces = render_result_new();
  VALUE has_children = Qfalse;
  int children_type = TYPE(children);

  if (html != Qnil) {
    render_result_write(pieces, RSTRING_PTR(html), RSTRING_LEN(html));
    has_children = Qtrue;
  } else if (children_type == T_STRING) {
    res = rb_funcall(self, id_encode_entities, 1, children);
    render_result_write(pieces, RSTRING_PTR(res), RSTRING_LEN(res));
    has_children = Qtrue;
  } else if (children_type == T_ARRAY) {
    VALUE child;
    len = RARRAY_LEN(children);
    for (i = 0; i < len; i++) {
      child = RARRAY_PTR(children)[i];
      if (child != Qnil && child != Qfalse) {
        VALUE child_svg_mode;
        if (strcmp(RSTRING_PTR(node_type_s), str_svg) == 0) {
          child_svg_mode = Qtrue;
        } else if (strcmp(RSTRING_PTR(node_type_s), str_foreign_object) == 0 && is_svg_mode == Qtrue) {
          child_svg_mode = Qfalse;
        } else {
          child_svg_mode = is_svg_mode;
        }
        size_t len = render_result_len(pieces);
        internal_render_to_string(child, context, child_svg_mode, select_value, self, pieces);
        if (len < render_result_len(pieces)) { has_children = Qtrue; }
      }
    }
  } else if (children != Qnil && children != Qfalse && children != Qtrue) {
    VALUE child_svg_mode;
    if (strcmp(RSTRING_PTR(node_type_s), str_svg) == 0) {
      child_svg_mode = Qtrue;
    } else if (strcmp(RSTRING_PTR(node_type_s), str_foreign_object) == 0 && is_svg_mode == Qtrue) {
      child_svg_mode = Qfalse;
    } else {
      child_svg_mode = is_svg_mode;
    }
    size_t len = render_result_len(pieces);
    internal_render_to_string(children, context, child_svg_mode, select_value, self, pieces);
    if (len < render_result_len(pieces)) { has_children = Qtrue; }
  }

  if (has_children == Qtrue) {
    render_result_concat(rres, pieces);
  } else if (rb_reg_match(rVoid, node_type_s) == Qtrue) {
    char *s = render_result_ptr(rres);
    char l = s[render_result_len(rres) - 1];
    if (l == '>') {
      render_result_resize(rres, render_result_len(rres) - 1);
      render_result_write(rres, " />", 3);
    }
    return;
  }

  render_result_free(pieces);

  render_result_write(rres, "</", 2);
  render_result_write(rres, RSTRING_PTR(node_type_s), RSTRING_LEN(node_type_s));
  render_result_write(rres, ">", 1);

  return;
}

VALUE render_to_string(int argc, VALUE *argv, VALUE self) {
  VALUE vnode, context, is_svg_mode, select_value, res;
  render_result_s *rres = render_result_new();
  rb_scan_args(argc, argv, "22", &vnode, &context, &is_svg_mode, &select_value);
  internal_render_to_string(vnode, context, is_svg_mode, select_value, self, rres);
  res = rb_str_new(render_result_ptr(rres), render_result_len(rres));
  render_result_free(rres);
  return res;
}

VALUE create_element(int argc, VALUE *argv, VALUE self) {
  VALUE type, props, children;
  rb_scan_args(argc, argv, "12", &type, &props, &children);
  VALUE normalized_props, key, ref;

  if (props != Qnil) {
    if (TYPE(props) == T_HASH) {
      normalized_props = rb_hash_dup(props);
      key = rb_hash_delete(normalized_props, sym_key);
      ref = rb_hash_delete(normalized_props, sym_ref);
    } else {
      children = props;
      normalized_props = rb_hash_new();
      key = Qnil;
      ref = Qnil;
    }
  } else {
    normalized_props = rb_hash_new();
    key = Qnil;
    ref = Qnil;
  }

  if (rb_block_given_p()) {
    VALUE pr = rb_funcall(mPreact, id_render_buffer, 0);
    pr = rb_ary_push(pr, rb_ary_new());
    VALUE block_result = rb_yield(Qnil);
    VALUE c = rb_ary_pop(pr);
    if (rb_funcall(mPreact, id_is_renderable, 1, block_result) == Qtrue) {
      rb_ary_push(c, block_result);
    }
    if (RARRAY_LEN(c) > 0) {
      children = c;
    }
  }

  if (children != Qnil) {
    rb_hash_aset(normalized_props, sym_children, children);
  }

  return rb_funcall(cVNode, id_new, 4, type, normalized_props, key, ref);
}

void Init_Preact(void) {
  id_checked = rb_intern("checked");
  id_children = rb_intern("children");
  id_class = rb_intern("class");
  id_class_name = rb_intern("class_name");
  id_class_name2 = rb_intern("className");
  id_context_id = rb_intern("context_id");
  id_context_type = rb_intern("context_type");
  id_dangerously_set_inner_html = rb_intern("dangerouslySetInnerHTML");
  id_default_checked = rb_intern("default_checked");
  id_default_selected = rb_intern("default_selected");
  id_default_value = rb_intern("default_value");
  id_development = rb_intern("development?");
  id_downcase = rb_intern("downcase");
  id_encode_entities = rb_intern("_encode_entities");
  id_eqeq = rb_intern("==");
  id_get_child_context = rb_intern("get_child_context");
  id_get_derived_state_from_props = rb_intern("get_derived_state_from_props");
  id_gsub = rb_intern("gsub");
  id_html_for = rb_intern("html_for");
  id_for = rb_intern("for");
  id_freeze = rb_intern("freeze");
  id_include = rb_intern("include?");
  id_is_renderable = rb_intern("is_renderable?");
  id_key = rb_intern("key");
  id_keyq = rb_intern("key?");
  id_keys = rb_intern("keys");
  id_merge = rb_intern("merge");
  id_new = rb_intern("new");
  id_props = rb_intern("props");
  id_ref = rb_intern("ref");
  id_render = rb_intern("render");
  id_render_buffer = rb_intern("render_buffer");
  id_selected = rb_intern("selected");
  id_self = rb_intern("__self");
  id_source = rb_intern("__source");
  id_start_withq = rb_intern("start_with?");
  id_style = rb_intern("style");
  id_style_obj_to_css = rb_intern("_style_obj_to_css");
  id_validate_props = rb_intern("validate_props");
  id_value = rb_intern("value");

  iv_context = rb_intern("@context");
  iv_declared_props = rb_intern("@declared_props");
  iv_next_state = rb_intern("@_nextState");
  iv_props = rb_intern("@props");
  iv_state = rb_intern("@state");

  sym_children = ID2SYM(rb_intern("children"));
  sym_class = ID2SYM(rb_intern("class"));
  sym_default = ID2SYM(rb_intern("default"));
  sym_for = ID2SYM(id_for);
  sym_html = ID2SYM(rb_intern("__html"));
  sym_key = ID2SYM(rb_intern("key"));
  sym_ref = ID2SYM(rb_intern("ref"));
  sym_selected = ID2SYM(rb_intern("selected"));
  sym_value = ID2SYM(id_value);

  mIsomorfeus = rb_const_get(rb_cObject, rb_intern("Isomorfeus"));
  mPreact = rb_define_module("Preact");
  cFragment = rb_define_class("Fragment", rb_cObject);

  // set instance variables on the module to keep references to these objects
  // to prevent garbage colloction
  rUnsafe = rb_reg_regcomp(rb_str_new_cstr(str_run));
  rb_obj_freeze(rUnsafe);
  rb_ivar_set(mPreact, rb_intern("@_r_unsafe"), rUnsafe);
  rVoid = rb_reg_regcomp(rb_str_new_cstr(str_rve));
  rb_obj_freeze(rVoid);
  rb_ivar_set(mPreact, rb_intern("@_r_void"), rVoid);
  rXlink = rb_reg_regcomp(rb_str_new_cstr(str_rxl));
  rb_obj_freeze(rXlink);
  rb_ivar_set(mPreact, rb_intern("@_r_xlink"), rXlink);
  rXlink2 = rb_reg_regcomp(rb_str_new_cstr(str_rx2));
  rb_obj_freeze(rXlink2);
  rb_ivar_set(mPreact, rb_intern("@_r_xlink2"), rXlink2);

  s_aria = rb_str_new_cstr("aria");
  rb_obj_freeze(s_aria);
  rb_ivar_set(mPreact, rb_intern("@_s_aria"), s_aria);
  s_false = rb_str_new_cstr("false");
  rb_obj_freeze(s_false);
  rb_ivar_set(mPreact, rb_intern("@_s_false"), s_false);
  s_true = rb_str_new_cstr("true");
  rb_obj_freeze(s_true);
  rb_ivar_set(mPreact, rb_intern("@_s_true"), s_true);
  s_xlink = rb_str_new_cstr("xlink:");
  rb_obj_freeze(s_xlink);
  rb_ivar_set(mPreact, rb_intern("@_s_xlink"), s_xlink);

  rb_define_module_function(mPreact, "_render_to_string", render_to_string, -1);
  rb_define_module_function(mPreact, "create_element", create_element, -1);
}
