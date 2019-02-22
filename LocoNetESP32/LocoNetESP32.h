/*
LocoNetESPSerial.h

Based on SoftwareSerial.cpp - Implementation of the Arduino software serial for ESP8266.
Copyright (c) 2015-2016 Peter Lerup. All rights reserved.

Adaptation to LocoNet (half-duplex network with DCMA) by Hans Tanner. 
See Digitrax LocoNet PE documentation for more information

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#ifndef LocoNetESP32_h
#define LocoNetESP32_h

//#include <inttypes.h>
#include <HardwareSerial.h>


// This class is compatible with the corresponding AVR one,
// the constructor however has an optional rx buffer size.
// Speed up to 115200 can be used.

#define lnBusy 0
#define lnAwaitBackoff 1
#define lnNetAvailable 2

#define rxBufferSize 64
#define txBufferSize 64
#define verBufferSize 48

class LocoNetESPSerial : public HardwareSerial
{
public:
   LocoNetESPSerial(int uartNr, int receivePin, int transmitPin, bool inverse_logic = false, unsigned int buffSize = 64);
   ~LocoNetESPSerial();
   void begin();
   int lnRead(void);
   size_t lnWrite(uint8_t);
   int lnAvailable(void);
   void processLoop();
   int cdBackoff();

   
private:
   
   // Member functions
   void sendBreakSequence();
   
   // Member variables
   int m_rxPin, m_txPin;
   bool m_invert;
   unsigned long m_bitTime;
   bool m_highSpeed;
   uint32_t m_StartCD;
   unsigned int rx_rdPos, rx_wrPos;
   unsigned int tx_rdPos, tx_wrPos;
   unsigned int ver_rdPos, ver_wrPos;
   uint16_t rx_buffer[rxBufferSize];
   uint8_t tx_buffer[txBufferSize];
   uint8_t ver_buffer[verBufferSize];

};


#endif
