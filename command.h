
#ifndef ____command__
#define ____command__


/* ChibiOS Includes */
#include "ch.h"
#include "hal.h"
/* ChibiOS Supplementary Includes */
#include "chprintf.h"
#include "shell.h"
/* FatFS */
#include "ff.h"
/* Project includes */
#include "fat.h"

void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]);
void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]);
void cmd_debug(BaseSequentialStream *chp, int argc, char *argv[]);
bool cmdGetDebug(void);
void cmdSetDebug(bool);
bool cmdIsShellRunning(void);
bool cmdIsShellTerminated(void) ;
void cmdShellCreate(void);
void cmdShellRelease(void);

#endif /* defined(____command__) */
