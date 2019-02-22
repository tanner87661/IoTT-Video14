  #define serialPort 2
  #define pinRx    22  //pin used to receive LocoNet signals
  #define pinTx    23  //pin used to transmit LocoNet signals
  #define LED_ON 1
  #define LED_OFF 0

  const int lnMaxMsgSize = 48; //max length of LocoNet message
  const int lnOutBufferSize = 10; //Size of LN messages that can be buffered in transmit buffer. Useful if network is congested

  #include <LocoNetESP32.h> //this is a modified version of SoftwareSerial, which includes LocoNet CD Backoff and Collision detection
  LocoNetESPSerial lnSerial(serialPort, pinRx, pinTx, true); //true is inverted signals

typedef struct {  //LocoNet receive buffer structure to receive messages from LocoNet
    bool    lnIsEcho = false;   //true: Echo; false: Regular message; 
    byte    lnStatus = 0;    //0: waiting for OpCode; 1: waiting for package data
    byte    lnBufferPtr = 0; //index of next msg buffer location to read
    byte    lnXOR = 0;
    byte    lnExpLen = 0;
    byte    lnData[lnMaxMsgSize];
} lnReceiveBuffer;    

lnReceiveBuffer lnInBuffer;

typedef struct {
    byte    lnMsgSize = 0;
    byte    lnData[48];
    uint16_t reqID = 0; //temporarily store reqID while waiting for message to get to head of buffer
} lnTransmitMsg;

typedef struct { //LocoNet transmit buffer structure to receive messages from MQTT and send to LocoNet
    byte commStatus; //busy, ready, transmit, collision, 
    uint32_t lastBusyEvent = 0;
    byte readPtr = 0;
    byte writePtr = 0;
    uint16_t reqID = 0; //store the reqID of the message currently on the way out
    uint32_t reqTime = 0;
    byte     reqOpCode = 0;
    lnTransmitMsg lnOutData[lnOutBufferSize]; //max of 10 messages in queue  
} lnTransmitQueue;

//Note: Messages sent to LocoNet will be echoed back into Receive buffer after successful transmission
lnTransmitQueue lnOutQueue;

void processLNValidMsg()
{
  if (lnInBuffer.lnIsEcho)  //this is an echo message confirming that sending was successful
    Serial.print("**");
  for (byte i=0; i <= lnInBuffer.lnBufferPtr; i++)
  {
    if (i>0)
      Serial.print(", ");
      Serial.print(lnInBuffer.lnData[i],16); //print in hex format
  }
  Serial.println();
}

void processLNError()
{
  
}

//read message byte from serial interface and process accordingly
void handleLNIn(uint16_t nextInt)
{
  byte nextByte = nextInt & 0x00FF;
  byte nextFlag = (nextInt & 0xFF00) >> 8;
  if ((nextByte & 0x00FF) >= 0x80) //start of new message
  {
    if (lnInBuffer.lnStatus == 1)
      processLNError();
    lnInBuffer.lnIsEcho = ((nextFlag & 0x01) > 0);  
    lnInBuffer.lnStatus = 1;
    lnInBuffer.lnBufferPtr = 0;
    byte swiByte = (nextByte & 0x60) >> 5;
    switch (swiByte)
    {
      case 0: lnInBuffer.lnExpLen  = 2; break;
      case 1: lnInBuffer.lnExpLen  = 4; break;
      case 2: lnInBuffer.lnExpLen  = 6; break;
      case 3: lnInBuffer.lnExpLen  = 0xFF; break;
      default: lnInBuffer.lnExpLen = 0;
    }
    lnInBuffer.lnXOR  = nextByte;
    lnInBuffer.lnData[0] = nextByte;
  }
  else
    if (lnInBuffer.lnStatus == 1) //collecting data
    {
      lnInBuffer.lnBufferPtr++; 
      lnInBuffer.lnData[lnInBuffer.lnBufferPtr] = nextByte;
      if ((lnInBuffer.lnBufferPtr == 1) && (lnInBuffer.lnExpLen == 0xFF))
      {
        lnInBuffer.lnExpLen  = nextByte & 0x007f;
      }
      if (lnInBuffer.lnBufferPtr == (lnInBuffer.lnExpLen - 1))
      {
        if ((lnInBuffer.lnXOR ^ 0xFF) == nextByte)
          processLNValidMsg();
        else
          processLNError();
        lnInBuffer.lnStatus = 0; //awaiting OpCode
      }  
      else
        lnInBuffer.lnXOR = lnInBuffer.lnXOR ^ nextByte;
    }
    else
    {
      //unexpected data byte. Ignore for the moment
      Serial.print("Unexpected Data: ");
      Serial.println(nextByte);
    }
      
}

//send message to LocoNet. Note that the message will echoed back and then be processed as incoming message
void handleLNOut()
{
  int next = (lnOutQueue.readPtr + 1) % lnOutBufferSize;
  byte firstOut = lnOutQueue.lnOutData[next].lnData[0];
  int msgSize = lnOutQueue.lnOutData[next].lnMsgSize;
      for (int j=0; j < msgSize; j++)  
      {
        if (lnSerial.lnWrite(lnOutQueue.lnOutData[next].lnData[j]) < 0)
        {
          //Buffer Overrun handling
          lnSerial.flush();
          Serial.println("Buffer Overrun detected");
          return;
        }
      }
      if ((lnOutQueue.lnOutData[next].lnData[0] & 0x08) > 0) //new OpCode, so if a request ID is given, we store it to match incoming answer with the request
      {
        lnOutQueue.reqID = lnOutQueue.lnOutData[next].reqID;
        lnOutQueue.reqTime = micros();
        lnOutQueue.reqOpCode = lnOutQueue.lnOutData[next].lnData[0];
      }
      else
      {
//        Serial.println("Set ReqID to zero"); //as no answer is expected from the command station for this message byte
        lnOutQueue.reqID = 0;
        lnOutQueue.reqOpCode = 0;
      }
      lnOutQueue.readPtr = next;
//      bytesTransmitted += msgSize; //this would be used to keep statistics
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Starting up");
  pinMode(pinRx, INPUT_PULLUP); //this shoud be set by HardwareSerial, but it seems it is not
}

void loop() {
  // put your main code here, to run repeatedly:
  lnSerial.processLoop();

/*
  if (lnSerial.lnAvailable())
  {
  while (lnSerial.lnAvailable() > 0)
  {
    Serial.print(lnSerial.lnRead());  
    Serial.print(" ");
  }
  Serial.println();
  }
*/  
  
  while (lnSerial.lnAvailable()) 
  {
    uint16_t thisByte = lnSerial.lnRead();
//    Serial.print(thisByte,16);
//    Serial.print(" ");
//    bytesReceived++; //this would be for statistical purposes
    handleLNIn(thisByte);
  }

  //never use the following read routine in a productive environment. You must add error checking etc.
  if (Serial.available())
  {
    lnTransmitMsg thisRecord; //Test String: 178, 30, 64, 19
    while (Serial.available())
    {
      String thisCmd = Serial.readStringUntil(',');
      byte newByte = byte(thisCmd.toInt());
      thisRecord.lnData[thisRecord.lnMsgSize] = newByte;
      thisRecord.lnMsgSize++;
    }
    int hlpPtr = (lnOutQueue.writePtr + 1) % lnOutBufferSize;
    lnOutQueue.lnOutData[hlpPtr] = thisRecord;
    lnOutQueue.writePtr = hlpPtr;
  }
  
  if (lnOutQueue.readPtr != lnOutQueue.writePtr) //something to send, so process it
  {
    handleLNOut();
  }
  
  switch (lnSerial.cdBackoff()) //update onboard LED based on LocoNet status. This may be slightly delayed, but who cares...
  {
    case lnBusy:
    {
      digitalWrite(LED_BUILTIN, LED_ON);
      break;
    }
    case lnNetAvailable:
    {
      digitalWrite(LED_BUILTIN, LED_OFF);
      break;
    }
    case lnAwaitBackoff:
    {
      digitalWrite(LED_BUILTIN, LED_ON);
      break;
    }
  }
}
