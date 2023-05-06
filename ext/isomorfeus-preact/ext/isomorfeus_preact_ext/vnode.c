#include <ruby.h>
#undef DEPRECATED
#define FIO_MEMORY_NAME vnode
#include "fio-stl.h"
#include "isomorfeus_preact_ext.h"

VALUE cVNode;

static void preact_vnode_mark(void *p) {
    VNode *v = (VNode *)p;
    rb_gc_mark(v->component);
    rb_gc_mark(v->key);
    rb_gc_mark(v->props);
    rb_gc_mark(v->ref);
    rb_gc_mark(v->type);
}

static void preact_vnode_free(void *p) {
  vnode_free(p);
}

static size_t preact_vnode_size(const void *p) {
  return sizeof(VNode);
  (void)p;
}

const rb_data_type_t preact_vnode_t = {
  .wrap_struct_name = "VNode",
  .function = {
    .dmark = preact_vnode_mark,
    .dfree = preact_vnode_free,
    .dsize = preact_vnode_size,
    .dcompact = NULL,
    .reserved = {0}
  },
  .parent = NULL,
  .data = NULL,
  .flags = RUBY_TYPED_FREE_IMMEDIATELY
};

static VALUE preact_vnode_alloc(VALUE rclass) {
  void *v = vnode_malloc(sizeof(VNode));
  return TypedData_Wrap_Struct(rclass, &preact_vnode_t, v);
}

static VALUE preact_vnode_init(VALUE self, VALUE type, VALUE props, VALUE key, VALUE ref) {
  VNode *v = (VNode *)DATA_PTR(self);
  v->component = Qnil;
  v->key = key;
  v->props = props;
  v->type = type;
  v->ref = ref;
  return self;
}

static VALUE preact_vnode_set_c(VALUE self, VALUE c) {
  VNode *v = (VNode *)DATA_PTR(self);
  v->component = c;
  return c;
}

static VALUE preact_vnode_get_c(VALUE self) {
  return ((VNode *)DATA_PTR(self))->component;
}

static VALUE preact_vnode_get_key(VALUE self) {
  return ((VNode *)DATA_PTR(self))->key;
}

static VALUE preact_vnode_get_props(VALUE self) {
  return ((VNode *)DATA_PTR(self))->props;
}

static VALUE preact_vnode_get_ref(VALUE self) {
  return ((VNode *)DATA_PTR(self))->ref;
}

static VALUE preact_vnode_get_type(VALUE self) {
  return ((VNode *)DATA_PTR(self))->type;
}

void Init_VNode(void) {
  cVNode = rb_define_class("VNode", rb_cObject);
  rb_define_alloc_func(cVNode, preact_vnode_alloc);
  rb_define_method(cVNode, "initialize", preact_vnode_init, 4);
  rb_define_method(cVNode, "component=", preact_vnode_set_c, 1);
  rb_define_method(cVNode, "component", preact_vnode_get_c, 0);
  rb_define_method(cVNode, "key", preact_vnode_get_key, 0);
  rb_define_method(cVNode, "props", preact_vnode_get_props, 0);
  rb_define_method(cVNode, "ref", preact_vnode_get_ref, 0);
  rb_define_method(cVNode, "type", preact_vnode_get_type, 0);
}
