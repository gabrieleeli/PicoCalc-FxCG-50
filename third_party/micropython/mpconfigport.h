/* Configurazione di MicroPython per la PicoCalc FxCG-50 (v1.17.0, solo
 * Pimoroni Pico Plus 2 W). Porta "embed" di MicroPython v1.24.1 (licenza MIT,
 * third_party/micropython/LICENSE). Scritta per questo progetto. */
#include <port/mpconfigport_common.h>

#define MICROPY_CONFIG_ROM_LEVEL                (MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES)
#define MICROPY_ENABLE_COMPILER                 (1)
#define MICROPY_ENABLE_GC                       (1)
#define MICROPY_PY_GC                           (1)
#define MICROPY_HELPER_REPL                     (1)
#define MICROPY_KBD_EXCEPTION                   (1)
#define MICROPY_FLOAT_IMPL                      (MICROPY_FLOAT_IMPL_DOUBLE)
#define MICROPY_LONGINT_IMPL                    (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_PY_MATH                         (1)
#define MICROPY_PY_BUILTINS_FLOAT               (1)
#define MICROPY_PY_BUILTINS_COMPLEX             (0)
#define MICROPY_PY_SYS                          (1)
#define MICROPY_PY_SYS_EXIT                     (1)
#define MICROPY_PY_SYS_PLATFORM                 "picocalc"
#define MICROPY_HW_BOARD_NAME                   "PicoCalc FxCG-50"
#define MICROPY_HW_MCU_NAME                     "RP2350"
#define MICROPY_PY_IO                           (0)
#define MICROPY_PY_STRUCT                       (0)
#define MICROPY_PY_COLLECTIONS                  (0)
#define MICROPY_PY_MICROPYTHON                  (0)
#define MICROPY_PY_ARRAY                        (0)
#define MICROPY_PY_ERRNO                        (0)
#define MICROPY_PY_BUILTINS_INPUT               (0)   // input() e' quello della PicoCalc (sotto)
#define MICROPY_PY_BUILTINS_HELP                (0)
#define MICROPY_ERROR_REPORTING                 (MICROPY_ERROR_REPORTING_NORMAL)
#define MICROPY_ENABLE_SOURCE_LINE              (1)
#define MICROPY_STACK_CHECK                     (1)
#define MICROPY_MODULE_FROZEN                   (0)
#define MICROPY_ENABLE_EXTERNAL_IMPORT          (0)
#define MICROPY_READER_POSIX                    (0)
#define MICROPY_ALLOC_PATH_MAX                  (64)

// Lavoro a passi: la VM chiama pc_py_poll() ogni MICROPY_VM_HOOK_COUNT salti e
// ritorni; pc_py_poll() restituisce il controllo al ciclo del firmware quando
// la fetta di tempo e' finita (python_port.cpp) e controlla AC.
#ifdef __cplusplus
extern "C" {
#endif
void pc_py_poll(void);
#ifdef __cplusplus
}
#endif
#define MICROPY_VM_HOOK_COUNT                   (32)
#define MICROPY_VM_HOOK_INIT                    static unsigned int vm_hook_divisor = MICROPY_VM_HOOK_COUNT;
#define MICROPY_VM_HOOK_POLL                    if (--vm_hook_divisor == 0) { vm_hook_divisor = MICROPY_VM_HOOK_COUNT; pc_py_poll(); }
#define MICROPY_VM_HOOK_LOOP                    MICROPY_VM_HOOK_POLL
#define MICROPY_VM_HOOK_RETURN                  MICROPY_VM_HOOK_POLL

// input(): riga digitata nella console della modalita' (modcasioplot.c).
struct _mp_obj_fun_builtin_var_t;
extern const struct _mp_obj_fun_builtin_var_t pc_input_obj;
#define MICROPY_PORT_BUILTINS                   { MP_ROM_QSTR(MP_QSTR_input), MP_ROM_PTR(&pc_input_obj) },

// I/O verso la console della modalita' Python (python_port.cpp).
#define mp_hal_stdin_rx_chr                     pc_py_stdin_rx_chr
#define mp_hal_stdout_tx_strn                   pc_py_stdout_tx_strn
