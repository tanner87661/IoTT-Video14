/*

SoftwareSerial.cpp - Implementation of the Arduino software serial for ESP8266.
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

#include <LocoNetESP32.h>
#include <HardwareSerial.h>

#define cdBackOffDelay 20  //20 bit-time Backoff Delay per LocoNet Standard
LocoNetESPSerial::LocoNetESPSerial(int uartNr, int receivePin, int transmitPin, bool inverse_logic, unsigned int buffSize) : HardwareSerial(uartNr) {
   m_invert = inverse_logic;
   m_rxPin = receivePin;
   m_txPin = transmitPin;
   m_StartCD = micros();

   m_rxPin = receivePin;
   rx_rdPos = rx_wrPos = 0;
   tx_rdPos = tx_wrPos = 0;
   ver_rdPos = ver_wrPos = 0;
//   pinMode(m_rxPin, INPUT_PULLUP); //needed to set this when using Software Serial. Seems to work here
   
   begin();
 }

LocoNetESPSerial::~LocoNetESPSerial() {
	
//	HardwareSerial::~HardwareSerial();
}

void LocoNetESPSerial::begin() {

   HardwareSerial::begin(16667, SERIAL_8N1, m_rxPin, m_txPin, m_invert);
   m_highSpeed = true;
   m_bitTime = 60; //round(1000000 / 16667); //60 uSecs

}

int LocoNetESPSerial::lnRead(void)
{
	if (lnAvailable() > 0)
	{
		int hlpInPtr = (rx_rdPos + 1) % rxBufferSize;
		int hlpRes = rx_buffer[hlpInPtr];
		rx_rdPos = hlpInPtr;
		return hlpRes;
	}
	else
	    return -1;
}

size_t LocoNetESPSerial::lnWrite(uint8_t c)
{
    int hlpOutPtr = (tx_wrPos + 1) % txBufferSize;
    if (hlpOutPtr != tx_rdPos) //override protection
    {
      tx_buffer[hlpOutPtr] = c;
      tx_wrPos = hlpOutPtr;
      return 1;
    }
    else
      return -1;
}

void LocoNetESPSerial::sendBreakSequence()
{
//	Serial.println(!m_invert ? LOW : HIGH);
    digitalWrite(m_txPin, !m_invert ? LOW : HIGH); //not sure this is working. The Serial port may not let me set the Tx pin from outside. Alternatives?
	uint32_t startSeq = micros();
	while (micros() < (startSeq + (15 * m_bitTime))) 
	{
		digitalWrite(m_txPin, !m_invert ? LOW : HIGH);
//		Serial.print('.');
	}
    digitalWrite(m_txPin, m_invert ? LOW : HIGH);
}

void LocoNetESPSerial::processLoop()
{
    if (digitalRead(m_rxPin) == !m_invert ? LOW : HIGH)
      m_StartCD = micros();
	while (HardwareSerial::available() > 0) //empty that input buffer
	{
		int hlpInPtr = (rx_wrPos + 1) % rxBufferSize;
		int hlpBuff = HardwareSerial::read();
//		Serial.print(ver_rdPos);
//		Serial.print(" ");
//		Serial.print(ver_wrPos);
//		Serial.println(" ");
		if (ver_rdPos != ver_wrPos)
		{
//			Serial.println("Echo");
			int hlpVerPos = (ver_rdPos + 1) % verBufferSize;
			if (ver_buffer[hlpVerPos] != hlpBuff)
			{
				Serial.println("collision detected");
				hlpBuff |= 0x0200;
				HardwareSerial::flush();
				sendBreakSequence();
			}
			ver_rdPos = hlpVerPos;
			hlpBuff |= 0x0100;
		}
	    rx_buffer[hlpInPtr] = hlpBuff;
	    rx_wrPos = hlpInPtr;
	}
	
	if ((tx_wrPos != tx_rdPos) && (cdBackoff() == lnNetAvailable))
	{
		int hlpRdPtr = (tx_rdPos + 1) % txBufferSize;
		int hlpVerPtr = (ver_wrPos + 1) % verBufferSize;
        if (digitalRead(m_rxPin) != !m_invert ? LOW : HIGH) //last check before sending
        {
			HardwareSerial::write(tx_buffer[hlpRdPtr]); //send first byte
			ver_buffer[hlpVerPtr] = tx_buffer[hlpRdPtr];
		    ver_wrPos = hlpVerPtr;
		    tx_rdPos = hlpRdPtr;
		    while (tx_wrPos != tx_rdPos) //now sending the rest back to back
		    {
				hlpRdPtr = (tx_rdPos + 1) % txBufferSize;
				hlpVerPtr = (ver_wrPos + 1) % verBufferSize;
				HardwareSerial::write(tx_buffer[hlpRdPtr]); //send next byte
				ver_buffer[hlpVerPtr] = tx_buffer[hlpRdPtr];
				ver_wrPos = hlpVerPtr;
				tx_rdPos = hlpRdPtr;
			}
//		Serial.print(ver_rdPos);
//		Serial.print(" ");
//		Serial.print(ver_wrPos);
//		Serial.println(" x");
		}
	}
}

int LocoNetESPSerial::lnAvailable(void)
{
	int numAvail = (rx_wrPos - rx_rdPos + rxBufferSize) % rxBufferSize;
	return numAvail;
}

int LocoNetESPSerial::cdBackoff() {
   if (digitalRead(m_rxPin) == !m_invert ? LOW : HIGH)
     return lnBusy;
   else
     if (m_StartCD + (m_bitTime * cdBackOffDelay) < micros())
       return lnNetAvailable;
     else
       return lnAwaitBackoff;
}

