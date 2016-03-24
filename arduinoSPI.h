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
#ifndef AFLIB_ARDUINOSPI_H
#define AFLIB_ARDUINOSPI_H

#include <SPI.h>
#include "afSPI.h"

class ArduinoSPI : public afSPI {
	public:

    ArduinoSPI(int chipSelect);
//afLib.cpp:    _spiSettings = SPISettings(1000000, LSBFIRST, SPI_MODE0);
        virtual void begin();
        virtual void beginSPI(); /* settings are in this class */
        virtual void endSPI();
        virtual void transfer(char *bytes,int len);

       private:

    SPISettings _spiSettings;
    int _chipSelect;
};

#endif


