/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
 */

/***** FATFS-MEMS **********/

#include <stdio.h>
#include <string.h>
/* ChibiOS Includes */
#include "ch.h"
#include "hal.h"
/* ChibiOS Supplementary Includes */
#include "chprintf.h"
/* Project includes */
#include "command.h"
#include "mems.h"
#include "led.h"
#include "serialUSB.h"


/* Virtual serial port over USB.*/
extern SerialUSBDriver SDU1;

/*===========================================================================*/
/* Main and generic code.                                                    */
/*===========================================================================*/


/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Shell manager initialization.
   */
  shellInit();
  
  sdcStart(&SDCD1, NULL);                       /* Start SD Driver */
  memsInit((BaseSequentialStream *)&SDU1);      /* Initializes the SPI driver 1 in order to access the MEMS */
  ledInit((BaseSequentialStream *)&SDU1);       /* Initializes the Led blinker */
  serialUSBInit((BaseSequentialStream *)&SDU1); /* Initializes the serial-over-USB CDC driver */

  serialUSBStart();
  memsStart();
  ledStart();
  
  /*
   * Set the thread name and set it to the lowest user priority
   * Since it is just going to be in a while loop.
   */
  chRegSetThreadName("main");
  chThdSetPriority(LOWPRIO);
  while (TRUE) {
    if (!cmdIsShellRunning()) {
      if (SDU1.config->usbp->state == USB_ACTIVE) {
        /* Spawns a new shell.*/
        cmdShellCreate();
      }
    }
    else {
      /* If the previous shell exited.*/
      if (cmdIsShellTerminated()) {
        /* Recovers memory of the previous shell.*/
        cmdShellRelease();
      }
    }
    chThdSleepMilliseconds(500);
  }
  return (int)NULL;
}
