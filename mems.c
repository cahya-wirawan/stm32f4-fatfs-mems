//
//  mems.c
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
/* ChibiOS Supplementary Includes */
#include "chprintf.h"
#include "lis302dl.h"

#include "mems.h"
#include "command.h"

BaseSequentialStream *memsSequentialStream;
int32_t memsX, memsY;

/*
 * SPI1 configuration structure.
 * Speed 5.25MHz, CPHA=1, CPOL=1, 8bits frames, MSb transmitted first.
 * The slave select line is the pin GPIOE_CS_SPI on the port GPIOE.
 */
static const SPIConfig memsSPI1Cfg = {
  NULL,
  /* HW dependent part.*/
  GPIOE,
  GPIOE_CS_SPI,
  SPI_CR1_BR_0 | SPI_CR1_BR_1 | SPI_CR1_CPOL | SPI_CR1_CPHA
};

void memsInit(BaseSequentialStream *stream){
  spiStart(&SPID1, &memsSPI1Cfg);
  lis302dlWriteRegister(&SPID1, LIS302DL_CTRL_REG1, 0x43);
  lis302dlWriteRegister(&SPID1, LIS302DL_CTRL_REG2, 0x00);
  lis302dlWriteRegister(&SPID1, LIS302DL_CTRL_REG3, 0x00);
  memsSequentialStream = stream;
}

/*
 * This is a periodic thread that reads accelerometer and save
 * the result on memsX and memsY
 */
static WORKING_AREA(memsThreadWA, 128);
static msg_t memsThread(void *arg) {
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
    memsX = ((int32_t)xbuf[0] + (int32_t)xbuf[1] +
            (int32_t)xbuf[2] + (int32_t)xbuf[3]) / 4;
    memsY = ((int32_t)ybuf[0] + (int32_t)ybuf[1] +
            (int32_t)ybuf[2] + (int32_t)ybuf[3]) / 4;
    if(counter%10 == 0) {
      palTogglePad(GPIOD, GPIOD_LED5);
      if(cmdGetDebug())
        chprintf(memsSequentialStream, "X:%d, Y:%d\r\n", memsX, memsY);
    }
    if(counter==1000000)
      counter=0;
    /* Waiting until the next 100 milliseconds time interval.*/
    chThdSleepUntil(time += MS2ST(100));
  }
  return (msg_t)NULL;
}

void memsStart(void){
  /*
   * Creates the Accelarator thread.
   */
  chThdCreateStatic(memsThreadWA, sizeof(memsThreadWA),
                    NORMALPRIO + 10, memsThread, NULL);
}

int32_t memsGetX(void){
  return memsX;
}

int32_t memsGetY(void){
  return memsY;
}

