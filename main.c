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

#include "usbcfg.h"

/* Virtual serial port over USB.*/
extern SerialUSBDriver SDU1;
int32_t accX, accY;

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
  static int32_t counter=0;
  
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
    
    counter++;
    
    if(SPID1.state != SPI_READY)
      continue;
    
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
      palTogglePad(GPIOD, GPIOD_LED5);
      if(cmdGetDebug())
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
  
  sdcStart(&SDCD1, NULL);       /* Start SD Driver */
  spiStart(&SPID1, &spi1cfg);   /* Initializes the SPI driver 1 in order to access the MEMS */
  
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
