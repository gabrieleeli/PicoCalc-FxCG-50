/* Colla in C tra MicroPython (porta embed) e src/python_port.cpp
 * (v1.17.0, PicoCalc FxCG-50). Scritta per questo progetto. Sostituisce
 * port/embed_util.c e port/mphalport.c del pacchetto embed: l'interprete gira
 * su una pila propria preparata da python_port.cpp. */
#include <string.h>
#include "py/compile.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "py/repl.h"
#include "py/runtime.h"
#include "py/stackctrl.h"
#include "shared/runtime/gchelper.h"

void pc_py_write(const char *s, size_t n);
void pc_py_fatal(void);

mp_uint_t pc_py_stdout_tx_strn(const char *str, size_t len) { pc_py_write(str, len); return len; }
void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) { pc_py_write(str, len); }
int pc_py_stdin_rx_chr(void) { return '\n'; }

void pc_py_glue_init(void *heap, size_t heap_size, void *stack_top, size_t stack_limit) {
    mp_stack_set_top(stack_top);
    mp_stack_set_limit(stack_limit);
    gc_init(heap, (uint8_t *)heap + heap_size);
    mp_init();
}
void pc_py_glue_deinit(void) {
    gc_sweep_all();
    mp_deinit();
}
/* 0 ok, 1 eccezione (stampata), 2 KeyboardInterrupt, 3 SystemExit. */
int pc_py_glue_exec(const char *src, size_t len, int single) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src, len, 0);
        qstr source_name = lex->source_name;
        mp_parse_tree_t parse_tree = mp_parse(lex, single ? MP_PARSE_SINGLE_INPUT : MP_PARSE_FILE_INPUT);
        mp_obj_t fun = mp_compile(&parse_tree, source_name, single);
        mp_call_function_0(fun);
        nlr_pop();
        return 0;
    }
    mp_obj_t exc = MP_OBJ_FROM_PTR(nlr.ret_val);
    const mp_obj_type_t *type = mp_obj_get_type(exc);
    if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(type), MP_OBJ_FROM_PTR(&mp_type_SystemExit))) return 3;
    const int kbd = mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(type), MP_OBJ_FROM_PTR(&mp_type_KeyboardInterrupt));
    mp_obj_print_exception(&mp_plat_print, exc);
    return kbd ? 2 : 1;
}
void pc_py_glue_interrupt(void) { mp_sched_keyboard_interrupt(); }
int pc_py_glue_continue(const char *text) { return mp_repl_continue_with_input(text) ? 1 : 0; }
void pc_py_glue_raise_interrupt(void) { mp_raise_type(&mp_type_KeyboardInterrupt); }

void gc_collect(void) {
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    gc_collect_end();
}
void nlr_jump_fail(void *val) {
    (void)val;
    pc_py_fatal();
    for (;;) {
    }
}
