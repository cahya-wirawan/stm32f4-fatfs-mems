#include <stdio.h>
#include <string.h>
#include "command.h"

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

/* Virtual serial port over USB.*/
SerialUSBDriver SDU1;
Thread *_cmd_shell = NULL;

bool _cmd_debug=FALSE;
bool _cmd_shell_running=FALSE;

#define SHELL_WA_SIZE   THD_WA_SIZE(2048)
#define TEST_WA_SIZE    THD_WA_SIZE(256)

void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
  size_t n, size;
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: mem\r\n");
    return;
  }
  n = chHeapStatus(NULL, &size);
  chprintf(chp, "core free memory : %u bytes\r\n", chCoreStatus());
  chprintf(chp, "heap fragments   : %u\r\n", n);
  chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
  static const char *states[] = {THD_STATE_NAMES};
  Thread *tp;
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: threads\r\n");
    return;
  }
  chprintf(chp, "    addr    stack prio refs     state time\r\n");
  tp = chRegFirstThread();
  do {
    chprintf(chp, "%.8lx %.8lx %4lu %4lu %9s %lu\r\n",
             (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
             (uint32_t)tp->p_prio, (uint32_t)(tp->p_refs - 1),
             states[tp->p_state], (uint32_t)tp->p_time);
    tp = chRegNextThread(tp);
  } while (tp != NULL);
}

void cmd_debug(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    chprintf(chp, "debug = %d\r\n", _cmd_debug);
    chprintf(chp, "Usage: debug <1|0>\r\n");
    return;
  }
  if (strcmp(argv[0], "1") == 0) {
    _cmd_debug = TRUE;
  }
  else if(strcmp(argv[0], "0") == 0) {
    _cmd_debug = FALSE;
  }
  chprintf(chp, "debug = %d\r\n", _cmd_debug);
}

bool cmdGetDebug() {
  return _cmd_debug;
}

void cmdSetDebug(bool debug) {
  _cmd_debug = debug;
}

static const ShellCommand commands[] = {
  {"mkfs", cmd_mkfs},
  {"mount", cmd_mount},
  {"unmount", cmd_unmount},
  {"tree", cmd_tree},
  {"free", cmd_free},
  {"mkdir", cmd_mkdir},
  {"hello", cmd_hello},
  {"cat", cmd_cat},
  {"mem", cmd_mem},
  {"threads", cmd_threads},
  {"debug", cmd_debug},
  {NULL, NULL}
};

static const ShellConfig _shell_cfg1 = {
  (BaseSequentialStream *)&SDU1,
  commands
};

bool cmdIsShellRunning() {
  return _cmd_shell_running;
}

bool cmdIsShellTerminated() {
  return chThdTerminated(_cmd_shell);
}

void cmdShellCreate() {
  _cmd_shell = shellCreate(&_shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
  _cmd_shell_running = TRUE;
}

void cmdShellRelease() {
  chThdRelease(_cmd_shell);
  _cmd_shell_running = FALSE;
}

