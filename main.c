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

/***** FATFS-01 **********/

#include <stdio.h>
#include <string.h>
/* ChibiOS Includes */
#include "ch.h"
#include "hal.h"
/* ChibiOS Supplementary Includes */
#include "chprintf.h"
#include "shell.h"
/* FatFS */
#include "ff.h"
#include "lis302dl.h"
/* Project includes */
#include "fat.h"

#include "usbcfg.h"

/* Virtual serial port over USB.*/
SerialUSBDriver SDU1;
int32_t accX, accY;
bool debug=FALSE;

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WA_SIZE(2048)
#define TEST_WA_SIZE    THD_WA_SIZE(256)

static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
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

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
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

static void cmd_debug(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    chprintf(chp, "debug = %d\r\n", debug);
    chprintf(chp, "Usage: debug <1|0>\r\n");
    return;
  }
  if (strcmp(argv[0], "1") == 0) {
    debug = TRUE;
  }
  else if(strcmp(argv[0], "0") == 0) {
    debug = FALSE;
  }
  chprintf(chp, "debug = %d\r\n", debug);
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

static const ShellConfig shell_cfg1 = {
  (BaseSequentialStream *)&SDU1,
  commands
};

/*
 * SPI1 configuration structure.
 * Speed 5.25MHz, CPHA=1, CPOL=1, 8bits frames, MSb transmitted first.
 * The slave select line is the pin GPIOE_CS_SPI on the port GPIOE.
 */
static const SPIConfig spi1cfg = {
  NULL,
  /* HW dependent part.*/
  GPIOE,
  GPIOE_CS_SPI,
  SPI_CR1_BR_0 | SPI_CR1_BR_1 | SPI_CR1_CPOL | SPI_CR1_CPHA
};

/*
 * This is a periodic thread that reads accelerometer and outputs
 * result to SPI2 and PWM.
 */
static WORKING_AREA(waThreadAccelerometer, 128);
static msg_t ThreadAccelerometer(void *arg) {
  static int8_t xbuf[4], ybuf[4];   /* Last accelerometer data.*/
  systime_t time;                   /* Next deadline.*/
  static int32_t counter;
  
  (void)arg;
  chRegSetThreadName("Accelerometer");
  
  /* LIS302DL initialization.*/
  lis302dlWriteRegister(&SPID1, LIS302DL_CTRL_REG1, 0x43);
  lis302dlWriteRegister(&SPID1, LIS302DL_CTRL_REG2, 0x00);
  lis302dlWriteRegister(&SPID1, LIS302DL_CTRL_REG3, 0x00);
  
  /* Reader thread loop.*/
  time = chTimeNow();
  while (TRUE) {
    unsigned i;
    
    /* Keeping an history of the latest four accelerometer readings.*/
    for (i = 3; i > 0; i--) {
      xbuf[i] = xbuf[i - 1];
      ybuf[i] = ybuf[i - 1];
    }
    
    /* Reading MEMS accelerometer X and Y registers.*/
    xbuf[0] = (int8_t)lis302dlReadRegister(&SPID1, LIS302DL_OUTX);
    ybuf[0] = (int8_t)lis302dlReadRegister(&SPID1, LIS302DL_OUTY);
    
    /* Calculating average of the latest four accelerometer readings.*/
    accX = ((int32_t)xbuf[0] + (int32_t)xbuf[1] +
            (int32_t)xbuf[2] + (int32_t)xbuf[3]) / 4;
    accY = ((int32_t)ybuf[0] + (int32_t)ybuf[1] +
            (int32_t)ybuf[2] + (int32_t)ybuf[3]) / 4;
    if(counter%5 == 0) {
      if(debug)
        chprintf((BaseSequentialStream *)&SDU1, "X:%d, Y:%d\r\n", accX, accY);
    }
    if(counter==1000000)
      counter=0;
    /* Waiting until the next 100 milliseconds time interval.*/
    chThdSleepUntil(time += MS2ST(100));
  }
  return (msg_t)NULL;
}

/*===========================================================================*/
/* Main and generic code.                                                    */
/*===========================================================================*/

/*
 * Green LED blinker thread to show the system is running.
 */
static WORKING_AREA(waThreadLed, 128);
static msg_t ThreadLed(void *arg) {
  (void)arg;
  chRegSetThreadName("blinker");
  while (TRUE) {
      palTogglePad(GPIOD, GPIOD_LED4);
      chThdSleep(MS2ST(500));
  }
  return (msg_t)NULL;
}

/*
 * Application entry point.
 */
int main(void) {
  Thread *shelltp = NULL;

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
  
  /*
   * Start SD Driver
   */
  sdcStart(&SDCD1, NULL);
  
  /*
   * Initializes a serial-over-USB CDC driver.
   */
  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);

  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * Note, a delay is inserted in order to not have to disconnect the cable
   * after a reset.
   */
  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(1000);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);
  
  /*
   * Initializes the SPI driver 1 in order to access the MEMS. The signals
   * are already initialized in the board file.
   */
  /*spiStart(&SPID1, &spi1cfg);*/

  /*
   * Creates the blinker thread.
   */
  chThdCreateStatic(waThreadLed, sizeof(waThreadLed), NORMALPRIO+10, ThreadLed, NULL);
  
  /*
   * Creates the Accelarator thread.
   */
  chThdCreateStatic(waThreadAccelerometer, sizeof(waThreadAccelerometer),
                    NORMALPRIO + 10, ThreadAccelerometer, NULL);
  
  /*
   * Set the thread name and set it to the lowest user priority
   * Since it is just going to be in a while loop.
   */
  chRegSetThreadName("main");
  chThdSetPriority(LOWPRIO);
  while (TRUE) {
    if (!shelltp) {
      if (SDU1.config->usbp->state == USB_ACTIVE) {
        /* Spawns a new shell.*/
        shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
      }
    }
    else {
      /* If the previous shell exited.*/
      if (chThdTerminated(shelltp)) {
        /* Recovers memory of the previous shell.*/
        chThdRelease(shelltp);
        shelltp = NULL;
      }
    }
    chThdSleepMilliseconds(500);
  }
  return (int)NULL;
}
