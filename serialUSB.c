//
//  serial.c
//  
//
//  Created by Cahya Wirawan on 17/11/14.
//
//

#include <stdio.h>
#include <string.h>
/* ChibiOS Includes */
#include "ch.h"
#include "hal.h"

#include "usbcfg.h"
#include "serialUSB.h"

BaseSequentialStream *serialSequentialStream;

void serialUSBInit(BaseSequentialStream *stream){
  serialSequentialStream = stream;
  /*
   * Initializes a serial-over-USB CDC driver.
   */
  sduObjectInit((SerialUSBDriver*) serialSequentialStream);
  sduStart((SerialUSBDriver*)serialSequentialStream, &serusbcfg);
}

void serialUSBStart(void){
  /*
   * Activates the USB driver and then the USB bus pull-up on D+.
   * Note, a delay is inserted in order to not have to disconnect the cable
   * after a reset.
   */
  usbDisconnectBus(serusbcfg.usbp);
  chThdSleepMilliseconds(1000);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);
}