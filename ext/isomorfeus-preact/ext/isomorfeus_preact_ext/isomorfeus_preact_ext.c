#include <ruby.h>

extern void Init_VNode(void);
extern void Init_Preact(void);

void Init_isomorfeus_preact_ext(void) {
    Init_VNode();
    Init_Preact();
}
