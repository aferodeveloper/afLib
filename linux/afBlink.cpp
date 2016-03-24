/**
 * Copyright 2015 Afero, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "Arduino.h"
#include <string>
#include "iafLib.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>

#include "gpiolib.h"
#include "examples/afBlink/profile/afBlink/device-description.h"

#include "linuxLog.h"
#include "linuxSPI.h"

iafLib *theLib = NULL;

long lastBlink = 0;
bool blinking = false;
bool moduloLEDIsOn = false;      // Track whether the Modulo LED is on
uint16_t moduleButtonValue = 1;  // Track the button value so we know when it has changed

// Modulo LED is active low
#define LED_OFF                   1
#define LED_ON                    0


void isr_callback()
{
  fprintf(stdout,"isr_callback\n");
}
void onAttributeSet_callback(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value)
{
  fprintf(stdout,"onAttributeSet_callback: %d len: %d %x %s\n",attributeId,valueLen,*value,value);
  switch (attributeId)
  {
        case AF_BLINK:
          blinking = (*value==1);
          fprintf(stdout,"blinking == %d\n",blinking);
        break;
        
  }
    if (theLib->setAttributeComplete(requestId, attributeId, valueLen, value) != afSUCCESS) {
        fprintf(stderr,"setAttributeComplete failed!");
    }

  
}
void onAttributeSetComplete_callback(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value)
{
  fprintf(stdout,"onAttributeSetComplete_callback: %d len: %d %s\n",attributeId,valueLen,value);
  fprintf(stdout,"onAttributeSetComplete_callback\n\n");
  fprintf(stdout,"onAttrSetComplete id: ");
  printf("%x ",attributeId);
    printf(" value: ");
    printf("%d\n",*value);
    switch (attributeId) {
        // Update the state of the LED based on the actual attribute value.
        case AF_MODULO_LED:
            moduloLEDIsOn = (*value == 0);
            break;

            // Allow the button on Modulo to control our blinking state.
        case AF_MODULO_BUTTON: {
            uint16_t *buttonValue = (uint16_t *) value;
            if (moduleButtonValue != *buttonValue) {
                moduleButtonValue = *buttonValue;
                blinking = !blinking;
                if (theLib->setAttribute(AF_BLINK, blinking) != afSUCCESS) {
                    fprintf(stderr,"Could not set BLINK\n");
                }
            }
        }
            break;

        default:
            break;
    }

}
void setModuloLED(int on) {
    if (moduloLEDIsOn != on) {
        int16_t attrVal = on ? LED_ON : LED_OFF; // Modulo LED is active low
        if (theLib->setAttribute(AF_MODULO_LED, attrVal) != afSUCCESS) {
            printf("Could not set LED\n");
        }
        moduloLEDIsOn = on;
    }
}



void toggleModuloLED() {
    setModuloLED(!moduloLEDIsOn);
}


int main(int argc, char *argv[])
{

        struct pollfd fdset[2];
        int nfds = 2;
        int gpio_fd, timeout, rc;
        char *buf[MAX_BUF];
        unsigned int interrupt_line= 17;/*17;*/
        unsigned int reset_line= 4;
        int len;
        int counter=0;

	Stream *theLog = new linuxLog();
        afSPI *theSPI = new linuxSPI();

        gpio_export(interrupt_line);
        gpio_set_dir(interrupt_line, 0);
        gpio_set_edge(interrupt_line, "falling");
        gpio_fd = gpio_fd_open(interrupt_line);

        timeout = POLL_TIMEOUT;

        fprintf(stdout,"Test\n");

        theLib = iafLib::create(0,isr_callback,onAttributeSet_callback,onAttributeSetComplete_callback,theLog,theSPI);
        theLib->mcuISR();

/* we need to hook up and use the reset line */
        gpio_export(reset_line);
        gpio_set_dir(reset_line, 1);
	gpio_set_value(reset_line,0);
    
        timespec sleep_time;
        timespec remaining;
        sleep_time.tv_sec=0;
        sleep_time.tv_nsec=250000;
        nanosleep(&sleep_time,&remaining);
         /* check for E_INTR? and call again? */
	gpio_set_value(reset_line,1);

        while (1) {
                counter++;
                memset((void*)fdset, 0, sizeof(fdset));

                fdset[0].fd = STDIN_FILENO;
                fdset[0].events = POLLIN;

                fdset[1].fd = gpio_fd;
                fdset[1].events = POLLPRI;

                lseek(gpio_fd, 0, SEEK_SET);    /* consume any prior interrupt */
                read(gpio_fd, buf, sizeof (buf));


                rc = poll(fdset, nfds, timeout);

                if (rc < 0) {
                        printf("\npoll() failed!\n");
                        return -1;
                }

                if (rc == 0) {
                        printf(".");
                }

                if (fdset[1].revents & POLLPRI) {
                        len = read(fdset[1].fd, buf, MAX_BUF);
                        printf("\npoll() GPIO %d interrupt occurred\n", interrupt_line);
                lseek(gpio_fd, 0, SEEK_SET);    /* consume interrupt */
                read(gpio_fd, buf, sizeof (buf));

                        theLib->mcuISR();
                }

                if (fdset[0].revents & POLLIN) {
                        (void)read(fdset[0].fd, buf, 1);
                        //printf("\npoll() stdin read 0x%2.2X\n", (unsigned int) buf[0]);
                }



                 if (blinking) {
                   if (counter % 20 ==0 ) {
                         toggleModuloLED();
                     }
                 } else {
                     setModuloLED(false);
                 }


                 theLib->loop();
                fflush(stdout);
        }

    gpio_fd_close(gpio_fd);
   return 0;
}




