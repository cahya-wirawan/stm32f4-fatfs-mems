//
//  led.c
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

#include "led.h"

BaseSequentialStream *ledSequentialStream;

/*
 * Green LED blinker thread to show the system is running.
 */
static WORKING_AREA(ledThreadWA, 128);
static msg_t ledThread(void *arg) {
  (void)arg;
  chRegSetThreadName("Led");
  while (TRUE) {
    palTogglePad(GPIOD, GPIOD_LED4);
    chThdSleep(MS2ST(500));
  }
  return (msg_t)NULL;
}

void ledInit(BaseSequentialStream *stream){
  ledSequentialStream = stream;
}

void ledStart(void){
  /*
   * Creates the Led thread.
   */
  chThdCreateStatic(ledThreadWA, sizeof(ledThreadWA),
                    NORMALPRIO + 10, ledThread, NULL);
}