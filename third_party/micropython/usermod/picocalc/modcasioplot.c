/* Modulo "casioplot" minimo per la PicoCalc FxCG-50 (v1.17.0): i nomi delle
 * funzioni seguono l'interfaccia descritta nel manuale fx-CG50 (cap. 17);
 * implementazione scritta per questo progetto. Il disegno passa a
 * python_port.cpp (pc_py_* in C), che lo inoltra alla modalita' Python. */
#include "py/runtime.h"
#include "py/obj.h"

void pc_py_set_pixel(int x, int y, int r, int g, int b);
int pc_py_get_pixel(int x, int y, int *r, int *g, int *b);
void pc_py_draw_string(int x, int y, const char *s, int r, int g, int b, int large);
void pc_py_clear_screen(void);
void pc_py_show_screen(void);
int pc_py_read_line(char *buf, int max);
void pc_py_glue_raise_interrupt(void);

static void color_of(mp_obj_t c, int *r, int *g, int *b) {
    size_t n;
    mp_obj_t *items;
    mp_obj_get_array(c, &n, &items);
    if (n != 3) mp_raise_ValueError(MP_ERROR_TEXT("color must be (r, g, b)"));
    *r = mp_obj_get_int(items[0]);
    *g = mp_obj_get_int(items[1]);
    *b = mp_obj_get_int(items[2]);
}
static mp_obj_t casioplot_set_pixel(size_t n_args, const mp_obj_t *args) {
    int r = 0, g = 0, b = 0;
    if (n_args > 2) color_of(args[2], &r, &g, &b);
    pc_py_set_pixel(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), r, g, b);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(casioplot_set_pixel_obj, 2, 3, casioplot_set_pixel);
static mp_obj_t casioplot_get_pixel(mp_obj_t x, mp_obj_t y) {
    int r, g, b;
    if (!pc_py_get_pixel(mp_obj_get_int(x), mp_obj_get_int(y), &r, &g, &b)) return mp_const_none;
    mp_obj_t t[3] = {mp_obj_new_int(r), mp_obj_new_int(g), mp_obj_new_int(b)};
    return mp_obj_new_tuple(3, t);
}
static MP_DEFINE_CONST_FUN_OBJ_2(casioplot_get_pixel_obj, casioplot_get_pixel);
static mp_obj_t casioplot_draw_string(size_t n_args, const mp_obj_t *args) {
    int r = 0, g = 0, b = 0, large = 1;
    if (n_args > 3) color_of(args[3], &r, &g, &b);
    if (n_args > 4) {
        const char *size = mp_obj_str_get_str(args[4]);
        large = size[0] != 's';
    }
    pc_py_draw_string(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), mp_obj_str_get_str(args[2]), r, g, b, large);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(casioplot_draw_string_obj, 3, 5, casioplot_draw_string);
static mp_obj_t casioplot_clear_screen(void) { pc_py_clear_screen(); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_0(casioplot_clear_screen_obj, casioplot_clear_screen);
static mp_obj_t casioplot_show_screen(void) { pc_py_show_screen(); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_0(casioplot_show_screen_obj, casioplot_show_screen);

// input([prompt]): la riga digitata nella console (la modalita' sospende lo
// script finche' l'utente non preme EXE).
static mp_obj_t pc_input(size_t n_args, const mp_obj_t *args) {
    if (n_args == 1) mp_obj_print(args[0], PRINT_STR);
    char buf[256];
    const int n = pc_py_read_line(buf, sizeof buf);
    if (n == -2) pc_py_glue_raise_interrupt();   /* AC durante input() */
    if (n < 0) mp_raise_type(&mp_type_EOFError);
    return mp_obj_new_str(buf, (size_t)n);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pc_input_obj, 0, 1, pc_input);

static const mp_rom_map_elem_t casioplot_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_casioplot) },
    { MP_ROM_QSTR(MP_QSTR_set_pixel), MP_ROM_PTR(&casioplot_set_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_pixel), MP_ROM_PTR(&casioplot_get_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_string), MP_ROM_PTR(&casioplot_draw_string_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear_screen), MP_ROM_PTR(&casioplot_clear_screen_obj) },
    { MP_ROM_QSTR(MP_QSTR_show_screen), MP_ROM_PTR(&casioplot_show_screen_obj) },
};
static MP_DEFINE_CONST_DICT(casioplot_globals, casioplot_globals_table);
const mp_obj_module_t casioplot_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&casioplot_globals,
};
MP_REGISTER_MODULE(MP_QSTR_casioplot, casioplot_module);
