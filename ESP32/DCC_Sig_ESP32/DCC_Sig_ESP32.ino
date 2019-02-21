
/*  ESP8266-12E / NodeMCU LocoNet Gateway

 *  Source code can be found here: https://github.com/tanner87661/LocoNet-MQTT-Gateway
 *  Copyright 2018  Hans Tanner IoTT
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 *  in compliance with the License. You may obtain a copy of the License at:
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  Unless required by applicable law or agreed to in writing, software distributed under the License is distributed
 *  on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License
 *  for the specific language governing permissions and limitations under the License.
 */
#include <stdlib.h>
#include <arduino.h>
#include <Math.h>

#define swVersion "V0.0.1"
#include <esp_task_wdt.h>
#include <NmraDcc.h>

#define sendLogMsg

#ifdef sendLogMsg
  #if defined(ESP8266)
    #include "EspSaveCrash.h" //standard library, install using library manager
    //https://github.com/espressif/arduino-esp32/issues/449
  #else
    #include <rom/rtc.h>
  #endif
#endif

  //#define ESP12LED 4 //onboard LED on ESP32 Chip Board
  //#define LED_BUILTIN 2  //onboard LED on NodeMCU development board. 
  #define serialPort 2
  #define pinRx    22  //pin used to receive LocoNet signals
  #define pinTx    23  //pin used to transmit LocoNet signals
  #define LED_ON 1
  #define LED_OFF 0
  #define NUM_LEDS 600 //typical # of LED's on a 2 x 5m strip 
  #define LED_DATA_PIN 12
  #define CTC_OUT_BUF_SIZE 20
  #define numPorts 1 //up to 64 button MUX inputs
  #define numTouchButtons 16

  #define DccInPin 4
  #define DccAckPin 15


//  #define MAX_LAMPS_PER_HEAD 6
//  #define MAX_ASPECTS 10
//  #define MAX_SIG_ADDR 4

#include <FastLED.h>

#include <LocoNetESP32.h> //this is a modified version of SoftwareSerial, which includes LocoNet CD Backoff and Collision detection
LocoNetESPSerial lnSerial(serialPort, pinRx, pinTx, true); //true is inverted signals
//LocoNetESPSerial lnSerial;//(serialPort, pinRx, pinTx, true); //true is inverted signals

#include <ArduinoJson.h> //standard JSON library, can be installed in the Arduino IDE

#include <FS.h> //standard File System library, can be installed in the Arduino IDE
#include "SPIFFS.h"
#define FORMAT_SPIFFS_IF_FAILED true

#include <TimeLib.h> //standard library, can be installed in the Arduino IDE
#include <NTPtimeESP.h> //NTP time library from Andreas Spiess, download from https://github.com/SensorsIot/NTPtimeESP
#include <EEPROM.h> //standard library, can be installed in the Arduino IDE

#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#define DNS__PORT  53
#include <DNSServer.h>

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

#include <RollAvgSmall.h> //used to calculate LocoNet network load

TaskHandle_t taskDCCInput;
TaskHandle_t taskHWSerial;
TaskHandle_t taskLEDCheck;
SemaphoreHandle_t swiBaton;
SemaphoreHandle_t sigBaton;

// SET YOUR NETWORK MODE TO USE WIFI
char netBIOSName[50] = "BlueBox";
#include <PubSubClient.h> //standard library, install using library manager

CRGB leds[NUM_LEDS];

char mqtt_server[50] = "broker.hivemq.com"; // = Mosquitto Server IP "192.168.xx.xx" as loaded from mqtt.cfg
uint16_t mqtt_port = 1883; // = Mosquitto port number, standard is 1883, 8883 for SSL connection;
char mqtt_user[50] = "";
char mqtt_password[50] = "";

char staticIP[50] = "";
char staticGateway[50] = "";
char staticNetmask[50] = "";
char staticDNS[50] = "";

char apName[50] = "";
char apGateway[50] = "";
char apPassword[50] = "";

#ifdef sendLogMsg
char lnLogMsg[] = "lnLog";
#endif
char lnPingTopic[] = "lnPing";  //ping topic, do not change. This is helpful to find Gateway IP Address if not known. 
char lnBCTopic[100] = "lnBC";  //default topic, can be specified in mqtt.cfg. Useful when sending messages from 2 different LocoNet networks
char lnEchoTopic[100] = "lnEcho"; //default topic, can be specified in mqtt.cfg

//char mqttMsg[800]; //buffer used to publish messages via mqtt

char ntpServer[50] = "us.pool.ntp.org"; //default server for US. Change this to the best time server for your region
NTPtime NTPch(ntpServer); 
int ntpTimeout = 5000; //ms timeout for NTP update request

//WiFiClientSecure wifiClientSec;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

//intervall NTP server is contacted to update time/date
const uint32_t ntpIntervallDefault = 86400000; //1 day in milliseconds. Store time in RTC of ESP32
const uint32_t ntpIntervallShort = 10000; //10 Seconds in case something went wrong
const uint32_t lnLoadIntervall = 1000; //1 Seconds to measure network load
const uint32_t lnPingIntervall = 10000; //300 Seconds to send ping string to MQTT server on Topic lnPing

strDateTime dateTime;
int timeZone = -5;

//Ajax Command sends a JSON with information about internal status
char ajaxCmdStr[] = "ajax_inputs";
char ajaxPingStr[] = "ajax_ping";
char ajaxDataStr[] = "ajax_data";

// OTHER VARIALBES
uint32_t ntpTimer = millis();
uint32_t lnLoadTimer = millis();
uint32_t lnPingTimer = millis();

int millisRollOver = 0; //used to keep track of system uptime after millis rollover
unsigned long lastMillis = 0;

RollAvgSmall networkLoad(20); //last 20 busy percentages, updated after each received message
unsigned long lastMessage = micros();
uint32_t  bytesReceived = 0;
uint32_t  bytesLastReceived = 0; //used for LocoNet Load Calculation
uint32_t  bytesTransmitted = 0;

File uploadFile;

NmraDcc  Dcc ;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocketClient * globalClient = NULL;
uint16_t wsRequest = 0;
DNSServer dnsServer;
//bool captiveMode = true;

#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager or install using library manager
AsyncWiFiManager wifiManager(&server,&dnsServer);

bool    useWifi = false;
bool    useAP = true;
bool    useDHCP = true;
bool    ntpOK = false;
bool    useNTP = false;
int     useInputMode = 0; //0: DCC 1: LocoNet 2: MQTT
bool    useBushby = false;
bool    isBushby = false;
bool    useEmulator = false;
bool    refreshAll = false;

uint32_t bushbyRefreshTimer = millis();// - 30000; //first time after 30 secs
const uint32_t bushbyRefreshIntv = 18000;//00; //check every 30 Minutes
const uint8_t maximumBrightness = 100; //percent
uint8_t globalBrightness = 50; //percent
uint16_t fadeCtrlSteps = 10; //# of steps of
const uint16_t fadeCtrlIntv = 20; //milliseconds

uint32_t ledUpdateTimer = millis();
const uint16_t ledUpdateIntv = 1000; //refresh LEDs at least once per second

const int lnMaxMsgSize = 48; //max length of LocoNet message
const int lnOutBufferSize = 10; //Size of LN messages that can be buffered after receipt from MQTT before sending to LocoNet. Useful if network is congested
const int lnRespTimeout = 500000; //microseconds

#define shortPressThreshold 200
#define longPressThreshold 1000

//////////////////////////////////////////////////////////////////////       Blue Box Data Types     /////////////////////////////////////////////////////////////////////
//data structures to read sensorconfig file
typedef struct {
  char colName[20];
  byte colRGB[3];
  word blinkRate;
} ledColorEntry;

ledColorEntry * ledColorList = NULL;
uint32_t ledColorListSize = 0;

//  "Brightness": {"AnalogInp":15, "Min":25, "Max": 3500},
typedef struct {
  word analogInp;
  word minVal;
  word maxVal;
} brightnessCtrlDef;
brightnessCtrlDef brightnessCtrl;

//  "NumLED": 108,
word ledChainLength = 0xFFFF;

uint16_t * sigAddressList = NULL;
uint16_t sigAddressListSize = 0;
/*
typedef struct {
  uint16_t width = 0;
  uint16_t height = 0;
  uint16_t radius = 0;
} panelDescriptor;
*/

typedef struct {
  uint8_t lampIndex = 0;
  CRGB targetColor = 0; //runtime
  CRGB oldTargetColor = 0; //runtime
  CRGB adjTargetColor = 0; //runtime
  int      fadeCtrlCtr = 0;
  uint8_t transMode = 0; // 1 byte
  int16_t blinkPeriod = 0; //runtime
  uint32_t blinkTimer = millis(); //runtime
  uint8_t  lampStatus = 0; //runtime
} lampDescriptor; //7 bytes total
lampDescriptor * lampDescriptorList = NULL;
uint16_t lampDescriptorListSize = 0;

typedef struct {
  uint32_t lampColor = 0;
  int16_t blinkPeriod = 0;
} aspectLampDescriptor;
aspectLampDescriptor * aspectLampDescriptorList = NULL;
uint16_t aspectLampDescriptorListSize = 0; //6  bytes total

typedef struct {
  uint8_t aspIntval[2] = {0,0};
  uint16_t aspectLampListStart = 0; //MAX_LAMPS_PER_HEAD lamps max
  uint8_t aspectLampListSize = 0;
} aspectDescriptor; //5 bytes total
aspectDescriptor * aspectDescriptorList = NULL;
uint16_t aspectDescriptorListSize = 0;

typedef struct {
  uint8_t sigAddrListSize = 0;  // 1 byte
  uint16_t sigAddrListStart = 0; //4 Addr, 8 bytes
  uint8_t addrMode = 0; // 1 byte
  uint8_t transMode = 0; // 1 byte
  uint16_t chgDelay = 0; // 2 bytes
  uint8_t chgStatus = 0;                //runtime
  uint32_t chgTrigTime = millis();      //runtime
  uint8_t  currentAspect = 0;           //runtime
  uint8_t sigLampDescriptorListSize = 0;
  uint16_t sigLampDescriptorListStart = 0;
  uint16_t sigAspectDescriptorListStart = 0; // 36 aspects max, 3 bytes + 6 bytes per lamp per aspect
  uint8_t sigAspectDescriptorListSize = 0;
} signalHeadCtrlDef; //19 bytes total

signalHeadCtrlDef * signalHeadCtrlDefList = NULL;
uint16_t signalHeadCtrlDefListSize = 0;

#define numSigs 2048
#define numSwis 512 //=2048/4
#define numBDs 512 //=4096/8

byte swiPos[numSwis]; //current status of switches, 4 per byte. First bit indicates correct state, second is position
byte swiOldPos[numSwis]; //current status of switches, 4 per byte. First bit indicates correct state, second is position
byte swiDuplPos[numSwis]; //current status of switches, 4 per byte. First bit indicates correct state, second is position
byte bdStatus[numBDs]; //4096 input bits
byte bdOldStatus[numBDs]; //4096 input bits
byte sigPos[numSigs]; //current status of aspects, 1 per byte values 0..31
byte sigOldPos[numSigs];
byte sigDuplPos[numSigs];

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

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
//  pinMode(ESP12LED, OUTPUT);
  pinMode(pinRx, INPUT_PULLUP); //this shoud be set by HardwareSerial, but it seems it is not
  digitalWrite(LED_BUILTIN, LED_OFF); //switch LEDs off (negative logic)
//  digitalWrite(ESP12LED, 1);

  for (int i=0; i<numSigs; i++)
  {
    sigPos[i] = 0;
    sigOldPos[i] = 0x1F;
  }

  Serial.println("Init SPIFFS");
  SPIFFS.begin(); //File System. Size is set to 1 MB during compile time

  Serial.println("Read Node Config");
  readNodeConfig(); //Reading configuration files from File System

  if (!useWifi)
  {
    useAP = true; //not possible to switch off both
    Serial.println("Init AP");
  }
  if (useWifi && useAP)
    WiFi.mode(WIFI_AP_STA);
  else
    if (useWifi)
      WiFi.mode(WIFI_STA);
    else
      WiFi.mode(WIFI_AP);

  if (useWifi)
  {
    Serial.println("Connect Wifi");
    connectToWifi();
  }
  else
  {
    useNTP = false;
    useInputMode = 0; //DCC
  }
  if (useAP)
  {
    createAccessPoint();
  }
 
  startWebServer();

  if (useInputMode == 0) //DCC
  {
    Serial.println("Configure for DCC");
  // Configure the DCC CV Programing ACK pin for an output
    pinMode(DccAckPin, OUTPUT);
  // Setup which External Interrupt, the Pin it's associated with that we're using and enable the Pull-Up 
    Dcc.pin(DccInPin, 1);
  // Call the main DCC Init function to enable the DCC Receiver
    Dcc.initAccessoryDecoder( MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE, 0 );
    Serial.println("DCC Init Done");
  }

  if (useInputMode == 2) //MQTT
  {
    MQTT_connect();
  }

  Serial.println("Read Signal Config");
  readSignalConfig();
  Serial.println("Read Config Completed");

//  if (ledChainLength == 0xFFFF)
//    ledChainLength = NUM_LEDS;
  if (ledChainLength > NUM_LEDS)  
    ledChainLength = NUM_LEDS;
  else
    if (ledChainLength < 10)
      ledChainLength = 10;
  FastLED.addLeds<WS2811, LED_DATA_PIN, GRB>(leds, ledChainLength);
  for (int i =0; i < NUM_LEDS; i++)
    leds[i] = 0;
  FastLED.show();
//  refreshAll = true;
#ifdef sendLogMsg
  sendLogMessage("Device Restart completed");
#endif

//Start Core 0 Task
  sigBaton = xSemaphoreCreateMutex();
  swiBaton = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(
    core0Loop, //task function
    "core0Loop", //task name
    2048, //task stack size
    NULL, //task parameter
    1, //priority
    &taskLEDCheck, //task handle
    0); //core nr
  delay(500);


Serial.println("Init Tasks");
  xTaskCreatePinnedToCore(
    hwSerialProcessLoop, //task function
    "hwSerialProcessLoop", //task name
    2048, //task stack size
    NULL, //task parameter
    1, //priority
    &taskHWSerial, //task handle
    1); //core nr
  delay(500);

  
  Serial.println("Setup complete");
}

bool createAccessPoint()
{
    Serial.println("Create Access Point");
    char cstringToParse[25];
    uint8_t ip[4];
    strcpy(cstringToParse, apGateway);
    sscanf(cstringToParse, "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
    IPAddress thisIP(ip[0],ip[1],ip[2],ip[3]);
    IPAddress thisNM(255,255,255,0);
    wifiManager.setAPStaticIPConfig(thisIP, thisIP, thisNM);
    WiFi.softAPConfig(thisIP, thisIP, thisNM);
    
    WiFi.softAP(apName,apPassword); //https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/soft-access-point-class.html
                                        //https://github.com/espressif/arduino-esp32/issues/985
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);      
  
}

bool connectToWifi()
{
  Serial.println("Connect to Station Access Point");
  char cstringToParse[25];
  uint8_t ip[4];
  strcpy(cstringToParse, apGateway);
  sscanf(cstringToParse, "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
  IPAddress thisIP(ip[0],ip[1],ip[2],ip[3]);
  IPAddress thisNM(255,255,255,0);
  wifiManager.setAPStaticIPConfig(thisIP, thisIP, thisNM);
  WiFi.softAPConfig(thisIP, thisIP, thisNM);

  if (!useDHCP)
  {
    strcpy(cstringToParse, staticIP);
    sscanf(cstringToParse, "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
    IPAddress thisStaticIP(ip[0],ip[1],ip[2],ip[3]);
    strcpy(cstringToParse, staticGateway);
    sscanf(cstringToParse, "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
    IPAddress thisStaticGateway(ip[0],ip[1],ip[2],ip[3]);
    strcpy(cstringToParse, staticDNS);
    sscanf(cstringToParse, "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
    IPAddress thisStaticDNS(ip[0],ip[1],ip[2],ip[3]);
    IPAddress thisStaticNM(255,255,255,0);
    wifiManager.setSTAStaticIPConfig(thisStaticIP, thisStaticGateway, thisStaticNM, thisStaticDNS);
  }
//  wifiManager.startConfigPortal("YellowBox", "IoTTCloud");
    wifiManager.autoConnect();

}

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return true;
  }


  void handleRequest(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/html");
    response->print("<!DOCTYPE html><html><head><title>Captive Portal</title></head><body>");
    response->print("<p>This is out captive portal front page.</p>");
    response->printf("<p>You were trying to reach: http://%s%s</p>", request->host().c_str(), request->url().c_str());
    response->printf("<p>Try opening <a href='http://%s'>this link</a> instead</p>", WiFi.softAPIP().toString().c_str());
    response->print("</body></html>");
//    captiveMode = false;
    request->send(response);
  }
 
};

 

void startWebServer()
{
    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

/*    server.on(ajaxDataStr, HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(handleJSON_Data()));
    });
*/
    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) 
    {
        String message;
        if (request->hasParam(ajaxCmdStr)) 
        {
            message = request->getParam(ajaxCmdStr)->value();
            Serial.println(message);
        }
        else
            Serial.println("no such message");
        request->send(200, "text/plain", String("handleJSON_Ping()"));
    });

/*    // Send a GET request to <IP>/get?message=<message>
    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String message;
        if (request->hasParam(ajaxCmdStr)) {
            message = request->getParam(ajaxCmdStr)->value();
//            #ifdef debugMode
              Serial.println(message);
//            #endif
            byte newCmd[message.length()+1];
            message.getBytes(newCmd, message.length()+1);     //add terminating 0 to make it work
//            if (processRflCommand(newCmd))          
//              request->send(200, "text/plain", String(handleJSON_Data()));
//            else
//              request->send(400, "text/plain", "Invalid Command");
        } else {
            message = "No message sent";
//            request->send(200, "text/plain", String(handleJSON_Ping()));
        }
    });
*/
    server.on("/post", HTTP_POST,[](AsyncWebServerRequest * request){}, NULL,[](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) 
    {
      Serial.println("receive post data");

      int headers = request->headers();
      int i;
      String fileName = "";
      for(i=0;i<headers;i++){
        AsyncWebHeader* h = request->getHeader(i);
        if (h->name() == "Content-Disposition")
        {
            int first = h->value().indexOf('"');
            int last =  h->value().indexOf('"', first+1);
            fileName = '/' + h->value().substring(first+1,last);
            if (!((first > 0) && (last > first)))
              return;
            Serial.printf("Found filename from %i to %i of ", first, last);
            Serial.println(fileName);
            break;
        }
      }
      if(!index)
      {
        uploadFile = SPIFFS.open(fileName.c_str(), "w");
        Serial.printf("UploadStart: %s\n", fileName.c_str());
      }
      int byteOK = uploadFile.write(data, len);
      Serial.printf("writing %i, %i bytes to: %s\n", len, byteOK, fileName.c_str());
      Serial.printf("Result: %i, %i \n", index, total);
      if ((index + len) == total)
      {
        uploadFile.close();
        Serial.printf("Upload Complete: %s\n", fileName.c_str());
        request->send(200, "text/plain", "Upload complete");
        ESP.restart();
      }
    });

    server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
      String hlpStr = "/" + filename;

      if(!index)
      {
        uploadFile = SPIFFS.open(hlpStr.c_str(), "w");
        Serial.printf("UploadStart: %s\n", filename.c_str());
      }
      int byteOK = uploadFile.write(data, len);
      Serial.printf("writing %i, %i bytes to: %s\n", len, byteOK, hlpStr.c_str());
      if(final)
      {
        uploadFile.close();
        Serial.printf("Upload Complete: %s\n", hlpStr.c_str());
        request->send(200, "text/plain", "Upload complete");
      }
    });

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      if(!index)
        Serial.printf("BodyStart: %u\n", total);
      Serial.printf("%s", (const char*)data);
      if(index + len == total)
        Serial.printf("BodyEnd: %u\n", total);
    });
    // Send a POST request to <IP>/post with a form field message set to <message>

    server.onNotFound([](AsyncWebServerRequest *request)
    {
      Serial.printf("%i NOT_FOUND: ", globalClient);
      if(request->method() == HTTP_GET)
        Serial.printf("GET");
      else if(request->method() == HTTP_POST)
        Serial.printf("POST");
      else if(request->method() == HTTP_DELETE)
        Serial.printf("DELETE");
      else if(request->method() == HTTP_PUT)
        Serial.printf("PUT");
      else if(request->method() == HTTP_PATCH)
        Serial.printf("PATCH");
      else if(request->method() == HTTP_HEAD)
        Serial.printf("HEAD");
      else if(request->method() == HTTP_OPTIONS)
        Serial.printf("OPTIONS");
      else
        Serial.printf("UNKNOWN");
      Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

      if(request->contentLength()){
        Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
        Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
      }
    });

//    dnsServer.start(53, "*", WiFi.softAPIP());
//    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);//only when requested from AP

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");
    server.begin();
}

/*
 * JSON structure for WS communication
 * LocoNet commands from LN to app and vice versa
 * LED on/off commands from App to CTC panel for testing/identification
 * Data load / save for config data and security element/BD/Switch/Route config data{}
 * Button report / emulation data
 */

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  Serial.printf("WS Event %i \n", globalClient);
  switch (type)
  {
    case WS_EVT_CONNECT:
    {
      globalClient = client;
      Serial.printf("Websocket client connection received from %u", client->id());
      break;
    }
    case WS_EVT_DISCONNECT:
    {
      globalClient = NULL;
      Serial.println("Client disconnected");
      break;
    }
    case WS_EVT_DATA:
    {
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      String msg = "";
      if(info->final && info->index == 0 && info->len == len)
      {
        //the whole message is in a single frame and we got all of it's data
//        Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

        if(info->opcode == WS_TEXT)
        {
          for(size_t i=0; i < info->len; i++) 
          {
            msg += (char) data[i];
          }
        } 
        else 
        {
          char buff[3];
          for(size_t i=0; i < info->len; i++) 
          {
            sprintf(buff, "%02x ", (uint8_t) data[i]);
            msg += buff ;
          }
        }
//        Serial.println(msg);
        DynamicJsonBuffer jsonBuffer(1500);
        JsonObject& root = jsonBuffer.parseObject(msg);
        if (!root.success())
        {
          Serial.println("Websocket parseObject() failed");
        }
        else
        {
          if (root.containsKey("Cmd"))
          {
            String newCmd = root["Cmd"];
            if (newCmd == "Switch")
            {
              if (root.containsKey("CmdParams"))
              {
                JsonArray& cmdParams = root["CmdParams"];
                for (int i=0;i<cmdParams.size();i++)
                {
                  String newParam = cmdParams[i];
                  if (newParam == "SendSwitch")
                    if (root.containsKey("JSONData"))
                    {
                      Serial.println("Send Switch Command");
                    }
                }
              }
            }

            
            if (newCmd == "Request")
            {
              if (root.containsKey("CmdParams"))
              {
                JsonArray& cmdParams = root["CmdParams"];
                for (int i=0;i<cmdParams.size();i++)
                {
                  String newParam = cmdParams[i];
                  if (newParam == "CfgData")
                    wsRequest |= 0x0001;
                  if (newParam == "SignalData")
                    wsRequest |= 0x0002;
                  if (newParam == "RuntimeData")
                    wsRequest |= 0x0004;
                }
              }
              
            }
            if (newCmd == "Display")
            {
              if (root.containsKey("CmdParams"))
              {
                JsonArray& cmdParams = root["CmdParams"];
                for (int i=0;i<cmdParams.size(); i++)
                {
                  String newParam = cmdParams[i];
                  if (newParam == "DisplayData")
                    if (root.containsKey("JSONData"))
                    {
                      Serial.println("Display JSON Request");
                      handleDisplayJSONRequests(root["JSONData"]);
                      //wsRequest |= 0x0008;
                    }
                  if (newParam == "DisplayTest")
                    if (root.containsKey("JSONData"))
                    {
                      Serial.println("DisplayTest JSON Request");
                      handleDisplayTestJSONRequests(root["JSONData"]);
                      //wsRequest |= 0x0008;
                    }
                  if (newParam == "Emulator")
                    if (root.containsKey("JSONData"))
                    {
                      Serial.println("Emulator JSON Request");
                      handleEmulatorJSONRequests(root["JSONData"]);
                      //wsRequest |= 0x0008;
                    }
                }
              }
            }
            if (newCmd == "Update")
            {
              Serial.println("Update");
              if (root.containsKey("CmdParams"))
              {
                JsonArray& cmdParams = root["CmdParams"];
                for (int i=0;i<cmdParams.size(); i++)
                {
                  String newParam = cmdParams[i];
                  if (newParam == "CfgData")
                    if (root.containsKey("JSONData"))
                    {
                      Serial.println("Config JSON Request");
                      handleConfigJSONRequests(root["JSONData"]);
                      wsRequest |= 0x0001;
                    }
                  if (newParam == "SignalData")
                    if (root.containsKey("JSONData"))
                    {
                      Serial.println("Signal JSON Request");
                      handleSignalJSONRequests(root["JSONData"]);
                      wsRequest |= 0x0002;
                    }
                }
              }
              
            }
          }
        }
//        Serial.printf("%s\n",msg.c_str());

//        if(info->opcode == WS_TEXT)
//          client->text("I got your text message");
//        else
//          client->binary("I got your binary message");
      } 
      else 
      {
        //message is comprised of multiple frames or the frame is split into multiple packets
        if(info->index == 0)
        {
          if(info->num == 0)
            Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
        }

        Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

        if(info->opcode == WS_TEXT){
          for(size_t i=0; i < info->len; i++) 
          {
            msg += (char) data[i];
          }
        } 
        else 
        {
          char buff[3];
          for(size_t i=0; i < info->len; i++) 
          {
            sprintf(buff, "%02x ", (uint8_t) data[i]);
            msg += buff ;
          }
        }
        Serial.printf("%s\n",msg.c_str());

        if((info->index + len) == info->len)
        {
          Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
          if(info->final)
          {
            Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
            if(info->message_opcode == WS_TEXT)
              client->text("I got your text message");
            else
              client->binary("I got your binary message");
          }
        }
      }
      break;
    }
    case WS_EVT_PONG:
    {
      break;
    }
    case WS_EVT_ERROR:
    {
      break;
    }
  }
}

void getInternetTime()
{
  int thisIntervall = ntpIntervallDefault;
  if (!ntpOK)
    thisIntervall = ntpIntervallShort;
  if (millis() > (ntpTimer + thisIntervall))
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("getInternetTime");
      uint32_t NTPDelay = millis();
      dateTime = NTPch.getNTPtime(timeZone, 2);
      ntpTimer = millis();
#ifdef sendLogMsg
      sendLogMessage("call getInternetTime()");
#endif
      while (!dateTime.valid)
      {
        delay(100);
        Serial.println("waiting for Internet Time");
        dateTime = NTPch.getNTPtime(timeZone, 2);
        if (millis() > NTPDelay + ntpTimeout)
        {
          ntpOK = false;
          Serial.println("Getting NTP Time failed");
          return;
        }
      }
      NTPDelay = millis() - NTPDelay;
#ifdef sendLogMsg
      sendLogMessage("call getInternetTime() complete");
#endif
      setTime(dateTime.hour, dateTime.minute, dateTime.second, dateTime.day, dateTime.month, dateTime.year);
      ntpOK = true;
      NTPch.printDateTime(dateTime);

      String NTPResult = "NTP Response Time [ms]: ";
      NTPResult += NTPDelay;
      Serial.println(NTPResult);
    }
    else
    {
#ifdef sendLogMsg
      sendLogMessage("No Internet when calling getInternetTime()");
#endif
      
    }
  }
}

#ifdef sendLogMsg
void sendLogMessage(const char* logMsg)
{
  if (useInputMode == 2) //MQTT
  {
    if (!mqttClient.connected())
    {
      Serial.println("Connect MQTT 514");
      MQTT_connect();
    }
    if (mqttClient.connected())
    {
      if (!mqttClient.publish(lnLogMsg, logMsg))
      {
        Serial.println(F("Log Failed"));
      } else {
//      Serial.println(F("Log OK!"));
      }
//    mqttClient.loop();
    }
  }
}
#endif
/*
int state ()
Returns the current state of the client. If a connection attempt fails, this can be used to get more information about the failure.

Returns
int - the client state, which can take the following values (constants defined in PubSubClient.h):
-4 : MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time
-3 : MQTT_CONNECTION_LOST - the network connection was broken
-2 : MQTT_CONNECT_FAILED - the network connection failed
-1 : MQTT_DISCONNECTED - the client is disconnected cleanly
0 : MQTT_CONNECTED - the client is connected
1 : MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT
2 : MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier
3 : MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection
4 : MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected
5 : MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect
*/
// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  // Loop until we're reconnected --  no, not anymore, see below
  while (!mqttClient.connected()) {
    if (WiFi.status() != WL_CONNECTED) 
    {
      WiFi.disconnect();
      WiFi.reconnect();
//      wifiManager.autoConnect();
//      WiFi.begin(staName, staPassword);
    }
    Serial.print("Attempting MQTT connection..." + String(mqtt_server) + " " + String(mqtt_port));
//    digitalWrite(ESP12LED, false);
    // Create a random client ID
    String clientId = "LNCTC" + String(random(99));// + ESP_getChipId();
    Serial.println(clientId);
    // Attempt to connect
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password)) 
    {
      Serial.println("connected");
      mqttClient.subscribe(lnBCTopic);
    } else {
      Serial.print("failed, rc= ");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(1000);
      return; //break the loop to make sure web server can be accessed to enter a valid MQTT server address
    }
  }
#ifdef sendLogMsg
//  sendLogMessage("MQTT Connected");
#endif
  Serial.println("MQTT Connected!");
//  digitalWrite(ESP12LED, true);
}

// This function is called whenever a normal DCC Turnout Packet is received and we're in Board Addressing Mode
//void notifyDccAccTurnoutBoard( uint16_t BoardAddr, uint8_t OutputPair, uint8_t Direction, uint8_t OutputPower )
//{
//  Serial.print("notifyDccAccTurnoutBoard: ") ;
//  Serial.print(BoardAddr,DEC) ;
//  Serial.print(',');
//  Serial.print(OutputPair,DEC) ;
//  Serial.print(',');
//  Serial.print(Direction,DEC) ;
//  Serial.print(',');
//  Serial.println(OutputPower, HEX) ;
//}

// This function is called whenever a normal DCC Turnout Packet is received and we're in Output Addressing Mode
void notifyDccAccTurnoutOutput( uint16_t Addr, uint8_t Direction, uint8_t OutputPower )
{
  uint16_t byteNr = trunc((Addr-1)/4);
  uint8_t inpPosStat = (Direction<<1) + OutputPower; //Direction and ON Status
  swiPos[byteNr] &= ~(0x03<<(2*((Addr-1) % 4)));
  swiPos[byteNr] |= inpPosStat<<(2*((Addr-1) % 4));
  Serial.print("notifyDccAccTurnoutOutput: ") ;
  Serial.print(Addr,DEC) ;
  Serial.print(',');
  Serial.print(Direction,DEC) ;
  Serial.print(',');
  Serial.println(OutputPower, HEX) ;
}

// This function is called whenever a DCC Signal Aspect Packet is received
void notifyDccSigOutputState( uint16_t Addr, uint8_t State)
{
  sigPos[Addr] = State;
  Serial.print("notifyDccSigOutputState: ") ;
  Serial.print(Addr,DEC) ;
  Serial.print(',');
  Serial.println(State, HEX) ;
}

//called when mqtt message with subscribed topic is received
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  DynamicJsonBuffer jsonBuffer(1000);
  JsonObject& root = jsonBuffer.parseObject(payload);
  if (!root.success())
  {
    Serial.println("nodeMCU parseObject() failed");
    return;
  }
  if (root.containsKey("From"))
  {
    String hlpStr = netBIOSName;
    hlpStr = hlpStr + "-" + String(ESP_getChipId());
    if (root["From"] == hlpStr)
    {
      return;
    }
  }
  if (root.containsKey("Data"))
  { 
    if (useInputMode == 2) //MQTT
    {
//      Serial.println((byte)root["Data"][0],16);
      switch ((byte)root["Data"][0])
      {
        case 0xB0: if (useBushby && isBushby) break; //OPC_SW_REQ
        case 0xB1:; //OPC_SW_REP
        case 0xBD:  //OPC_SW_ACK
        {
          uint16_t swiAddr = (((byte)root["Data"][1] & 0x7F)) + (((byte)root["Data"][2] & 0x0F)<<7);
          Serial.print("Swi Change ");
          Serial.println(swiAddr);
          uint16_t byteNr = trunc(swiAddr/4);
          uint8_t inpPosStat = ((byte)root["Data"][2] & 0x30)>>4; //Direction and ON Status
          inpPosStat |=0x01;
          xSemaphoreTake(swiBaton, portMAX_DELAY);
          swiPos[byteNr] &= ~(0x03<<(2*(swiAddr % 4)));
          swiPos[byteNr] |= inpPosStat<<(2*(swiAddr % 4));
          xSemaphoreGive(swiBaton);
//          Serial.println(swiPos[byteNr]);
          if (globalClient != NULL)
            jsonEventReport(11, swiAddr, inpPosStat);
          break;
        }
        case 0xB2: //OPC_INPUT_REP
        {
          uint16_t inpAddr = (((byte)root["Data"][1] & 0x7F)<<1) + (((byte)root["Data"][2] & 0x0F)<<8) + (((byte)root["Data"][2] & 0x20)>>5);
//          Serial.print("BD Change ");
//          Serial.println(inpAddr);
          uint16_t byteNr = trunc(inpAddr/8);
          byte bitMask = 0x01<<(inpAddr % 8);
//          Serial.print(" ");
//          Serial.print(byteNr);
//          Serial.print(" ");
//          Serial.println(bitMask);
//          Serial.print(bdStatus[byteNr]);
//          Serial.print("->");
          uint8_t inpStatus = ((byte)root["Data"][2] & 0x10)>>4;
          if (inpStatus > 0)
            bdStatus[byteNr] |= bitMask;
          else
            bdStatus[byteNr] &= ~bitMask;
//          Serial.println(bdStatus[byteNr]);
          if (globalClient != NULL)
            jsonEventReport(10, inpAddr, inpStatus);
          break;
        }
        case 0xED: //OPC_IMM_PACKET
        {
          if (((byte)root["Data"][1] == 0x0B) && ((byte)root["Data"][2] == 0x7F))
          {
            byte recData[6];
            for (int i=0; i < 5; i++)
            {
              recData[i] = (byte)root["Data"][i+4];
              if (i > 0)
                recData[i] |= ((recData[0] & (0x01<<(i-1)))<<(8-i)); //distribute 8th  its to data bytes
            }
            word boardAddress = (((~recData[2]) & 0x70) << 2) | (recData[1] & 0x3F) ;
            byte turnoutPairIndex = (recData[2] & 0x06) >> 1;
            word sigAddress = (((boardAddress-1)<<2) | turnoutPairIndex) + 1;
            word sigAspect = recData[3] & 0x1F;
            Serial.printf("Setting Signal %i to Aspect %i \n", sigAddress, sigAspect);
//            Serial.print(recData[0],16);
//            Serial.print(" ");
//            Serial.println(recData[1],16);
            xSemaphoreTake(sigBaton, portMAX_DELAY);
            sigPos[sigAddress] = sigAspect;
            xSemaphoreGive(sigBaton);
          }
          break;
        }
        case 0xEF: //OPC_WR_SL
        case 0xE7: //OPC_SL_RD
        {
//          Serial.println("Receive SL_RD");
          if (((byte)root["Data"][1] == 0x0E) && ((byte)root["Data"][2] == 0x7F)) //Status slot with OpSw
          {
            isBushby = (((byte)root["Data"][6] & 0x04) > 0);
            Serial.printf("Bushby Bit is %i \n", isBushby);
          }
          break;
        }
      }
    }
  }
}

void handlePingMessage()
{
  char pingMqttMsg[200];
  if (useInputMode == 2) //MQTT
  {
    String hlpStr = handlePingJSON();
    hlpStr.toCharArray(pingMqttMsg, hlpStr.length()+1);
    if (!mqttClient.connected())
    {
      Serial.println("Connect MQTT 651");
      MQTT_connect();
    }
    if (mqttClient.connected())
    {
      if (!mqttClient.publish(lnPingTopic, pingMqttMsg)) 
      {
        Serial.println(F("Ping Failed"));
      } else {
        Serial.println(F("Ping OK!"));
        Serial.println(String(ESP.getFreeHeap()));
      }
//    mqttClient.loop();
    }
  }
}

void processLNError()
{
/*  if (useInputMode == 1) //LocoNet
  {
    DynamicJsonBuffer jsonBuffer(1200);
    String jsonOut = "";
    JsonObject& root = jsonBuffer.createObject();
    String hlpStr = netBIOSName;
    hlpStr = hlpStr + "-" + String(ESP_getChipId());
    root["From"] = hlpStr; //NetBIOSName + "-" + ESP_getChipId();
    root["Valid"] = 0;
    root["Time"] = millis();
    JsonArray& data = root.createNestedArray("Data");
    for (byte i=0; i <= lnInBuffer.lnBufferPtr; i++)
    {
      data.add(lnInBuffer.lnData[i]);
    }
    lnInBuffer.lnStatus = 0;
    root.printTo(mqttMsg);
    if (!mqttClient.connected())
    {
      Serial.println("Connect MQTT 687");
      MQTT_connect();
    }
    if (mqttClient.connected())
    {
//    xSemaphoreTake(mqttBaton, portMAX_DELAY);
      if (lnInBuffer.lnIsEcho)  //send echo message if echo flag is set 
        if (!mqttClient.publish(lnEchoTopic, mqttMsg))
        {
          Serial.println(F("lnEcho Failed"));
        } else {
//          Serial.println(F("lnEcho OK!"));
        }
      else  //otherwise send BC message (in direct mode, meaning the command came in via lnOutTopic)
        if (!mqttClient.publish(lnBCTopic, mqttMsg))
        {
          Serial.println(F("lnIn Failed"));
        } else {
//        Serial.println(F("lnIn OK!"));
        }
//    xSemaphoreGive(mqttBaton);
    }
  } */
}

void processLNValidMsg()
{
#ifdef sendLogMsg
      sendLogMessage("processLNValidMsg()");
#endif
//  Serial.println("Process Valid Message");
  if (useInputMode == 1) //LocoNet
  {
      switch (lnInBuffer.lnData[0])
      {
        case 0xB0: if (useBushby && isBushby) break; //OPC_SW_REQ
        case 0xB1:; //OPC_SW_REP
        case 0xBD:  //OPC_SW_ACK
        {
          uint16_t swiAddr = ((lnInBuffer.lnData[1] & 0x7F)) + ((lnInBuffer.lnData[2] & 0x0F)<<7);
          uint16_t byteNr = trunc(swiAddr/4);
          uint8_t inpPosStat = (lnInBuffer.lnData[2] & 0x30)>>4; //Direction and ON Status
          inpPosStat |=0x01;
          xSemaphoreTake(swiBaton, portMAX_DELAY);
          swiPos[byteNr] &= ~(0x03<<(2*(swiAddr % 4)));
          swiPos[byteNr] |= inpPosStat<<(2*(swiAddr % 4));
          xSemaphoreGive(swiBaton);
          if (globalClient != NULL)
            jsonEventReport(11, swiAddr, inpPosStat);
          break;
        }
        case 0xB2: //OPC_INPUT_REP
        {
          uint16_t inpAddr = ((lnInBuffer.lnData[1] & 0x7F)<<1) + ((lnInBuffer.lnData[2] & 0x0F)<<8) + ((lnInBuffer.lnData[2] & 0x20)>>5);
          uint16_t byteNr = trunc(inpAddr/8);
          byte bitMask = 0x01<<(inpAddr % 8);
          uint8_t inpStatus = (lnInBuffer.lnData[2] & 0x10)>>4;
          if (inpStatus > 0)
            bdStatus[byteNr] |= bitMask;
          else
            bdStatus[byteNr] &= ~bitMask;
//          Serial.println(bdStatus[byteNr]);
          if (globalClient != NULL)
            jsonEventReport(10, inpAddr, inpStatus);
          break;
        }
        case 0xED: //OPC_IMM_PACKET
        {
          if ((lnInBuffer.lnData[1] == 0x0B) && (lnInBuffer.lnData[2] == 0x7F))
          {
            byte recData[6];
            for (int i=0; i < 5; i++)
            {
              recData[i] = lnInBuffer.lnData[i+4]; //get all the IMM bytes
              if (i > 0)
                recData[i] |= ((recData[0] & (0x01<<(i-1)))<<(8-i)); //distribute 8th  its to data bytes
            }
            word boardAddress = (((~recData[2]) & 0x70) << 2) | (recData[1] & 0x3F) ;
            byte turnoutPairIndex = (recData[2] & 0x06) >> 1;
            word sigAddress = (((boardAddress-1)<<2) | turnoutPairIndex) + 1;
            word sigAspect = recData[3] & 0x1F;
            Serial.printf("Setting Signal %i to Aspect %i from ", sigAddress, sigAspect);
            Serial.print(recData[0],16);
            Serial.println(recData[1],16);
            xSemaphoreTake(sigBaton, portMAX_DELAY);
            sigPos[sigAddress] = sigAspect;
            xSemaphoreGive(sigBaton);
          }
        }
        case 0xEF: //OPC_WR_SL
        case 0xE7: //OPC_SL_RD
        {
          if ((lnInBuffer.lnData[1] == 0x0E) && (lnInBuffer.lnData[2] == 0x7F)) //Status slot with OpSw
          {
            isBushby = ((lnInBuffer.lnData[6] & 0x04) > 0);
          }
          break;
        }
      }
  }

#ifdef sendLogMsg
      sendLogMessage("processLNValidMsg() done");
#endif
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

//send message received from mqtt topic to LocoNet. Note that the message will e echoed back and then be processed as incoming message
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
      if ((lnOutQueue.lnOutData[next].lnData[0] & 0x08) > 0)
      {
        lnOutQueue.reqID = lnOutQueue.lnOutData[next].reqID;
        lnOutQueue.reqTime = micros();
        lnOutQueue.reqOpCode = lnOutQueue.lnOutData[next].lnData[0];
//        Serial.print("Set ReqID to outgoing after sending: ");
//        Serial.print(lnOutQueue.reqID);
//        Serial.print(" ");
//        Serial.println(lnOutQueue.reqOpCode);
      }
      else
      {
//        Serial.println("Set ReqID to zero");
        lnOutQueue.reqID = 0;
        lnOutQueue.reqOpCode = 0;
      }
      lnOutQueue.readPtr = next;
      bytesTransmitted += msgSize;
}

byte getInpStatus(int inpNr)
{
  word byteNr = trunc(inpNr/8);
  byte bitMask = 0x01 << (inpNr % 8);
  bool currStatus = (bdStatus[byteNr] & bitMask) > 0;
  bool oldStatus = (bdOldStatus[byteNr] & bitMask) > 0;
  byte retByte = 0;
  if (currStatus)
    retByte |= 0x01;
  if (currStatus != oldStatus)
    retByte |= 0x02;
  return retByte;
}

void updateInpStatus(int inpNr)
{
  word byteNr = trunc(inpNr/8);
  byte bitMask = 0x01 << (inpNr % 8);
  if ((bdStatus[byteNr] & bitMask) > 0)
    bdOldStatus[byteNr] |= bitMask;
  else
    bdOldStatus[byteNr] &= ~bitMask;
}

byte getSwiStatus(uint16_t swiNr)
{
  word byteNr = swiNr >> 2; //trunc(swiNr/4);
  byte bitMask = 0x03 << (2*(swiNr % 4));
  byte currStatus = (swiDuplPos[byteNr] & bitMask);
  byte oldStatus = (swiOldPos[byteNr] & bitMask);
  byte retByte = currStatus>>(2*(swiNr % 4));
  if ((currStatus != oldStatus) || ((currStatus & 0x01)> 0))
    retByte |= 0x04;
//  Serial.printf("getSwi %i Byte %i Bit %i Current %i Old %i Return %i \n", swiNr, byteNr, bitMask, currStatus, oldStatus, retByte);    
  return retByte;
}

//void updateSwiStatus(uint16_t swiNr)
//{
//  word byteNr = swiNr >> 2;
//  byte bitMask = 0x03 << (2*(swiNr % 4));
//  byte currStatus = (swiPos[byteNr] & bitMask);
//  swiOldPos[byteNr] &= (~bitMask);
//  swiOldPos[byteNr] |= currStatus;
//}

byte getSigStatus(uint16_t sigNr)
{
  byte retByte = sigDuplPos[sigNr];
  if (sigDuplPos[sigNr] != sigOldPos[sigNr])
    retByte |= 0x80;
  return retByte;
}

//byte updateSigStatus(uint16_t sigNr)
//{
//  sigOldPos[sigNr] = sigPos[sigNr];
//}

void sendLocoNetCommand(lnTransmitMsg thisRecord)
{
  char myMQTTMsg[200];
  if (useInputMode == 2) //MQTT
  {
    DynamicJsonBuffer jsonBuffer(1200);
    String jsonOut = "";
    JsonObject& root = jsonBuffer.createObject();
    String hlpStr = netBIOSName;
    hlpStr = hlpStr + "-" + String(ESP_getChipId());
    root["From"] = hlpStr; //NetBIOSName + "-" + ESP_getChipId();
    root["Valid"] = 1;
    root["Time"] = millis();
    JsonArray& data = root.createNestedArray("Data");
    for (byte i=0; i < thisRecord.lnMsgSize; i++)
      data.add(thisRecord.lnData[i]);
    root.printTo(myMQTTMsg);
    Serial.println(myMQTTMsg);
    if (!mqttClient.connected())
      MQTT_connect();
    if (mqttClient.connected())
    {
      if (!mqttClient.publish(lnBCTopic, myMQTTMsg))
        Serial.println(F("lnIn Failed"));
    }
  }
  if (useInputMode == 1) //LocoNet
  {
    int hlpPtr = (lnOutQueue.writePtr + 1) % lnOutBufferSize;
    lnOutQueue.lnOutData[hlpPtr] = thisRecord;
    lnOutQueue.writePtr = hlpPtr;
  }
}

void sendRequestSlotByNr(byte slotNr)
{
  lnTransmitMsg thisRecord;
  thisRecord.lnMsgSize = 4;
  thisRecord.lnData[0] = 0xBB;
  thisRecord.lnData[1] = (byte)slotNr;
  thisRecord.lnData[2] = 0x00;
  thisRecord.lnData[3] = (thisRecord.lnData[0] ^ thisRecord.lnData[1] ^ thisRecord.lnData[2] ^ 0xFF);
  sendLocoNetCommand(thisRecord);
}

void refreshBushby()
{
  sendRequestSlotByNr(0x7F);  
}

void jsonEventReport(int eventType, int eventIdentifier, int eventValue)
{
  if (globalClient != NULL)
  {
    String JSONStr = "{\"evtType\":" + String(eventType) + ",\"evtID\":" + String(eventIdentifier) + ", \"evtVal\":" + String(eventValue) + "}";
    Serial.println("Send Socket: " + JSONStr);
    globalClient->text("{\"Cmd\": \"Report\",\"CmdParams\": [\"EventData\"],\"JSONData\":" + JSONStr + "}");
  }
}
//evtTypes:  0: buttonPressed Val 1/0; 1: buttonClicked no Val; 2: buttonLongPressed Val 1/0; 3: analogVal
//evtTypes: 10: Block Detector Val 1/0; 11: Switch Status Val 0,1,2,3; 

void handleWSRequests()
{
  if (globalClient != NULL)
  {
    if ((wsRequest & 0x0001) > 0)
    {
      globalClient->text("{\"Cmd\": \"Report\",\"CmdParams\": [\"CfgData\"],\"JSONData\": " + handleConfigJSON() + "}");
      wsRequest &= (!0x0001);
    }
    if ((wsRequest & 0x0002) > 0)
    {
      globalClient->text("{\"Cmd\": \"Report\",\"CmdParams\": [\"SignalData\"],\"JSONData\": " + handleSignalJSON(true) + "}");
      wsRequest &= (!0x0002);
    }
    if ((wsRequest & 0x0004) > 0)
    {
      globalClient->text("{\"Cmd\": \"Report\",\"CmdParams\": [\"RuntimeData\"],\"JSONData\": " + handleRuntimeJSON() + "}");
      wsRequest &= (!0x0004);
    }
  }
  else
    wsRequest = 0;
}

void hwSerialProcessLoop(void * parameter)
{
  while (true)
  {
    esp_task_wdt_reset();
    lnSerial.processLoop();
  }
}

void showSignalData(signalHeadCtrlDef * thisEntry)
{
  Serial.println("Lamp Descriptors:");
  for (int i = 0; i < thisEntry->sigLampDescriptorListSize; i++)
  {
    lampDescriptor * thisLamp = &lampDescriptorList[thisEntry->sigLampDescriptorListStart + i];
    Serial.printf("LampDef %i for Index %i \n",i,thisLamp->lampIndex);
  }

  Serial.println("Lamp Descriptors:");
  for (int i = 0; i < thisEntry->sigAspectDescriptorListSize; i++)
  {
    aspectDescriptor * thisAspect = &aspectDescriptorList[thisEntry->sigAspectDescriptorListStart + i];
    Serial.printf("Loading Aspect %i [%i] [%i] %i %i \n", thisEntry->sigAspectDescriptorListStart + i, thisAspect->aspIntval[0], thisAspect->aspIntval[1], thisAspect->aspectLampListStart, thisAspect->aspectLampListSize);
    for (int j = 0; j < thisAspect->aspectLampListSize; j++)
    {
      aspectLampDescriptor * thisAspectLamp = &aspectLampDescriptorList[thisAspect->aspectLampListStart + j];
      Serial.printf("Triyng LED %i to %i with Blinkrate %i\n", thisEntry->sigLampDescriptorListStart + j, thisAspectLamp->lampColor, thisAspectLamp->blinkPeriod);
    }
    
  }
}

CRGB adjColor (CRGB origCol, uint8_t newBrightness)
{
  CRGB retVal;
  retVal.r = origCol.r * newBrightness / maximumBrightness;
  retVal.g = origCol.g * newBrightness / maximumBrightness;
  retVal.b = origCol.b * newBrightness / maximumBrightness;
  return retVal;
}

void processLEDOutput()
{
  bool needUpdate = false;
  for (int i = 0; i < lampDescriptorListSize; i++)
  {
    lampDescriptor * thisLamp = &lampDescriptorList[i];
    thisLamp->adjTargetColor = adjColor(thisLamp->targetColor, globalBrightness);
    CRGB currCol = leds[i];
    if (thisLamp->blinkPeriod != 0) //LED is blinking
    {
      if ((thisLamp->blinkTimer + abs(thisLamp->blinkPeriod)) < millis())
      {
        thisLamp->blinkTimer = millis();
        thisLamp->lampStatus ^= 0x01; //set or clear blink bit
        if (thisLamp->blinkPeriod > 0)
          leds[i] = (((thisLamp->lampStatus) & 0x01) > 0) ? thisLamp->adjTargetColor : CRGB::Black;
        else
          leds[i] = (((thisLamp->lampStatus) & 0x01) > 0) ? CRGB::Black : thisLamp->adjTargetColor;
        thisLamp->oldTargetColor = leds[i]; //to avoid wrong start color when fading after blinking
        needUpdate = needUpdate || (currCol != leds[i]);
//        if (currCol != leds[i])
//          Serial.printf("Update Blink LED %i \n", i);
      }
    }
    else 
      if (thisLamp->transMode > 0)
      {
        if ((thisLamp->blinkTimer + fadeCtrlIntv) < millis())
        {
         thisLamp->blinkTimer = millis();
         if (thisLamp->fadeCtrlCtr < 0) //fading out
         {
           thisLamp->adjTargetColor = adjColor(thisLamp->oldTargetColor, globalBrightness * -thisLamp->fadeCtrlCtr / fadeCtrlSteps);
           thisLamp->fadeCtrlCtr++;
         }
         else 
           if (thisLamp->fadeCtrlCtr == 0) //changing colors
           {
             thisLamp->adjTargetColor = 0;
             thisLamp->oldTargetColor = thisLamp->targetColor;
             thisLamp->fadeCtrlCtr++;
           }
           else
             if (thisLamp->fadeCtrlCtr < fadeCtrlSteps)
             {
               thisLamp->adjTargetColor = adjColor(thisLamp->targetColor, globalBrightness * thisLamp->fadeCtrlCtr / fadeCtrlSteps);
               thisLamp->fadeCtrlCtr++;
             }
             else
               thisLamp->adjTargetColor = adjColor(thisLamp->targetColor, globalBrightness);
//          Serial.println(thisLamp->adjTargetColor,16);
          leds[i] = thisLamp->adjTargetColor;
          needUpdate = needUpdate || (currCol != leds[i]);
//        if (currCol != leds[i])
//          Serial.printf("Update Trans LED %i \n", i);
        }
      }
      else
      {
        leds[i] = thisLamp->adjTargetColor;
        needUpdate = needUpdate || (currCol != leds[i]);
//        if (currCol != leds[i])
//          Serial.printf("Update Direct LED %i \n", i);
      }        
  }
  if (needUpdate || ((ledUpdateTimer + ledUpdateIntv) < millis()))
  {
//    Serial.println("UpdateLEDs");
    FastLED.show();
    ledUpdateTimer = millis();
  }
}

void loadAspect(signalHeadCtrlDef * thisEntry)
{
//  showSignalData(thisEntry);

  word aspOfs = 0;
  aspectDescriptor * thisAspect = NULL;
  if (thisEntry->addrMode == 2) //aspect has an interval range, so we must find the corresponding aspect
  {
    for (int i = 0; i < thisEntry->sigAspectDescriptorListSize; i++)
    {
      thisAspect = &aspectDescriptorList[thisEntry->sigAspectDescriptorListStart + i];
      aspOfs = i;
      if ((thisAspect->aspIntval[0] <= thisEntry->currentAspect) && (thisAspect->aspIntval[1] >= thisEntry->currentAspect)) //aspect found
        break;
    }
  }
  else
  {
    aspOfs = thisEntry->currentAspect;
    thisAspect = &aspectDescriptorList[thisEntry->sigAspectDescriptorListStart + thisEntry->currentAspect];
  }
//  Serial.printf("Loading Aspect %i [%i] [%i] %i %i \n", thisEntry->sigAspectDescriptorListStart + aspOfs, thisAspect->aspIntval[0], thisAspect->aspIntval[1], thisAspect->aspectLampListStart, thisAspect->aspectLampListSize);
  if (thisAspect != NULL)   
  {  
//    Serial.printf("Setting aspect with %i LEDs starting at %i \n", thisAspect->aspectLampListSize, thisAspect->aspectLampListStart);
    for (int j = 0; j < thisAspect->aspectLampListSize; j++)
    {
      lampDescriptor * thisLamp = &lampDescriptorList[thisEntry->sigLampDescriptorListStart + j];
      aspectLampDescriptor * thisAspectLamp = &aspectLampDescriptorList[thisAspect->aspectLampListStart + j];
      thisLamp->targetColor = thisAspectLamp->lampColor;
      thisLamp->blinkPeriod = thisAspectLamp->blinkPeriod;
      thisLamp->transMode = thisEntry->transMode;
      thisLamp->fadeCtrlCtr = - fadeCtrlSteps; //initialize fade control counter in case fade mode is soft
    }
  }
  else
    Serial.println("No Aspect");
}

void core0Loop(void * parameter)
{
  byte needUpdate; 
  bool doRefresh = refreshAll;
  refreshAll = false;
  while (true)
  {
    for (int i = 0; i < numSigs; i++)
    {
      xSemaphoreTake(sigBaton, portMAX_DELAY);
      sigDuplPos[i] = sigPos[i];
      xSemaphoreGive(sigBaton);
    }
    for (int i = 0; i < numSwis; i++)
    {
      xSemaphoreTake(swiBaton, portMAX_DELAY);
      swiDuplPos[i] = swiPos[i];
      swiPos[i]&= 0xAA; //clear dynamic flags
      xSemaphoreGive(swiBaton);
    }
    for (int i = 0; i < signalHeadCtrlDefListSize; i++)
    {
        signalHeadCtrlDef * thisEntry = &signalHeadCtrlDefList[i];
        switch (thisEntry->addrMode)
        {
          case 0: //dynamic switch
            needUpdate = 0;
            for (int j = 0; j < thisEntry->sigAddrListSize; j++)
            {
              needUpdate = getSwiStatus(sigAddressList[thisEntry->sigAddrListStart+j]-1);
              if ((needUpdate > 0x03) || doRefresh)
              {
                uint8_t tempAspect = (2 * j) + ((needUpdate>>1) & 0x01);
                thisEntry->currentAspect = tempAspect;
                thisEntry->chgStatus = 0;
//                updateSwiStatus(sigAddressList[thisEntry->sigAddrListStart+j]-1);
                loadAspect(thisEntry);
//                Serial.printf("update dynamic switch signal %i to aspect %i\n", i, tempAspect);
                break;
              }
              
            }
            break; 
          case 1: //static switch
            needUpdate = 0;
            for (int j = 0; j < thisEntry->sigAddrListSize; j++)
               needUpdate |= getSwiStatus(sigAddressList[thisEntry->sigAddrListStart+j]-1);
            if ((needUpdate > 0x03) || doRefresh)
            {
              thisEntry->chgTrigTime = millis();
              thisEntry->chgStatus = 1;
//              for (int j = 0; j < thisEntry->sigAddrListSize; j++)
//                updateSwiStatus(sigAddressList[thisEntry->sigAddrListStart+j]-1);
            }
            if (((thisEntry->chgStatus > 0) && ((thisEntry->chgTrigTime + thisEntry->chgDelay) < millis())) || refreshAll)
            { //change the LED aspect
              uint8_t tempAspect = 0;
              for (int j = 0; j < thisEntry->sigAddrListSize; j++)
                tempAspect = tempAspect + (((getSwiStatus(sigAddressList[thisEntry->sigAddrListStart+j]-1) & 0x02)>>1)<<j);
              thisEntry->currentAspect = tempAspect;
              thisEntry->chgStatus = 0;
              loadAspect(thisEntry);
//              Serial.printf("update static switch signal %i to aspect %i\n", i, tempAspect);
            }
            break;
          case 2: //static signal
            needUpdate = getSigStatus(sigAddressList[thisEntry->sigAddrListStart]-1);
            if ((needUpdate > 0x1F) || doRefresh)
            {
              thisEntry->currentAspect = (needUpdate & 0x1F);
              thisEntry->chgStatus = 0;
              Serial.printf("update direct signal %i pos %i to aspect %i\n", i, thisEntry->sigAddrListStart, thisEntry->currentAspect);
              loadAspect(thisEntry);
//              updateSigStatus(sigAddressList[thisEntry->sigAddrListStart]-1);
            }
            break;
        }
    }
    for (int i = 0; i < numSigs; i++)
      sigOldPos[i] = sigDuplPos[i];
    for (int i = 0; i < numSwis; i++)
    {
      swiDuplPos[i]&= 0xAA;
      swiOldPos[i] = swiDuplPos[i];
    }
    if (!useEmulator)
      processLEDOutput();
  }
}

void loop() //core1Loop
{
#if defined(ESP8266)
  ESP.wdtFeed();
#else
  //add ESP32 code
#endif

//  if (captiveMode)
//    dnsServer.processNextRequest();
    
  if (millis() < lastMillis)
  {
    millisRollOver++;
  //in case of Rollover, update all other affected timers
    ntpTimer = millis();
    lnLoadTimer = millis();
    lnPingTimer = millis();
    bushbyRefreshTimer = millis();
#ifdef sendLogMsg
      sendLogMessage("millis() rollover");
#endif
  }
  else
    lastMillis = millis(); 
     
  if ((bushbyRefreshTimer + bushbyRefreshIntv) < millis())
  {
    bushbyRefreshTimer = millis();
    if (useBushby)
    {
//      Serial.println("Check Bushby Bit");
      refreshBushby();    
    }
  }
  
  if (useInputMode == 0) //DCC
  {
    Dcc.process();
  }

  if (useInputMode == 2) //MQTT
  {
    if (mqttClient.connected())
      mqttClient.loop();
    else
    {
      Serial.println("Connect MQTT 1325");
      MQTT_connect();
    }
  }
  handleWSRequests();

  if (useNTP)
    getInternetTime();

//  server.handleClient();

  if (millis() > lnPingTimer + lnPingIntervall)                           // only for development. Please change it to longer interval in production
  {
    handlePingMessage();
    lnPingTimer = millis();
  }

  if (millis() > lnLoadTimer + lnLoadIntervall)                           // here we measure network load of LocoNet
  {
    float bytesDiff = bytesReceived - bytesLastReceived;
    networkLoad.update(bytesDiff);
    bytesLastReceived = bytesReceived;
    lnLoadTimer = millis();
  }
  if (useInputMode == 1) //LocoNet
    while (lnSerial.lnAvailable()) 
    {
     uint16_t thisByte = lnSerial.lnRead();
     bytesReceived++;
     handleLNIn(thisByte);
    }
  
  if (useInputMode == 1) //LocoNet
  {
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
} //loop

// function to check existence of nested key see https://github.com/bblanchon/ArduinoJson/issues/322
bool containsNestedKey(const JsonObject& obj, const char* key) {
    for (const JsonPair& pair : obj) {
        if (!strcmp(pair.key, key))
            return true;

        if (containsNestedKey(pair.value.as<JsonObject>(), key)) 
            return true;
    }

    return false;
}

//read node config file with variable settings
int readNodeConfig()
{
  DynamicJsonBuffer jsonBuffer(1000);
  if (SPIFFS.exists("/node.cfg"))
  {
    File dataFile = SPIFFS.open("/node.cfg", "r");
    if (dataFile)
    {
      Serial.print("Reading Node Config File ");
      Serial.println(dataFile.size());
      String jsonData;
      while (dataFile.position()<dataFile.size())
      {
        jsonData = jsonData + dataFile.readStringUntil('\n');
        jsonData.trim();
      } 
      dataFile.close();
      Serial.println("Reading Node Done");
      Serial.println(jsonData);
      
      JsonObject& root = jsonBuffer.parseObject(jsonData);
      if (root.success())
      {
        if (root.containsKey("useWifi"))
          useWifi = bool(root["useWifi"]);
        if (root.containsKey("NetBIOSName"))
          strcpy(netBIOSName, root["NetBIOSName"]);
//        if (root.containsKey("staSSID"))
//          strcpy(staName, root["staSSID"]);
//        if (root.containsKey("staPassword"))
//          strcpy(staPassword, root["staPassword"]);
        if (root.containsKey("useDHCP"))
          useDHCP = bool(root["useDHCP"]);
        if (root.containsKey("staticIP"))
          strcpy(staticIP, root["staticIP"]);
        if (root.containsKey("staticGateway"))
          strcpy(staticGateway, root["staticGateway"]);
        if (root.containsKey("staticNetmask"))
          strcpy(staticNetmask, root["staticNetmask"]);
        if (root.containsKey("staticDNS"))
          strcpy(staticDNS, root["staticDNS"]);
        if (root.containsKey("useAP"))
          useAP = bool(root["useAP"]);
        if (root.containsKey("apName"))
          strcpy(apName, root["apName"]);
        if (root.containsKey("apGateway"))
          strcpy(apGateway, root["apGateway"]);
//        if (root.containsKey("apUser"))
 //         strcpy(apUser, root["apUser"]);
        if (root.containsKey("apPassword"))
          strcpy(apPassword, root["apPassword"]);
        if (root.containsKey("useNTP"))
          useNTP = bool(root["useNTP"]);
        if (root.containsKey("NTPTimeServer"))
        {
//          String hlpStr = root["NTPTimeServer"];
          strcpy(ntpServer, root["NTPTimeServer"]); // = hlpStr.c_str();
          NTPch.setNTPServer(ntpServer);
        }
        if (root.containsKey("ntpTimeZone"))
          timeZone = int(root["ntpTimeZone"]);
        if (root.containsKey("useBushby"))
          useBushby = bool(root["useBushby"]);
        if (root.containsKey("useInputMode"))
          useInputMode = root["useInputMode"];
        if (root.containsKey("mqttServer"))
        {
          if (containsNestedKey(root, "ip"))
            strcpy(mqtt_server, root["mqttServer"]["ip"]);
          if (containsNestedKey(root, "port"))
            mqtt_port = uint16_t(root["mqttServer"]["port"]);
          if (containsNestedKey(root, "user"))
            strcpy(mqtt_user, root["mqttServer"]["user"]);
          if (containsNestedKey(root, "password"))
            strcpy(mqtt_password, root["mqttServer"]["password"]);
          Serial.print(mqtt_server);
          Serial.print(" Port ");
          Serial.println(mqtt_port);
        }
        if (root.containsKey("lnBCTopic"))
        {
          strcpy(lnBCTopic, root["lnBCTopic"]);
        }
        if (root.containsKey("lnEchoTopic"))
        {
          strcpy(lnEchoTopic, root["lnEchoTopic"]);
        }
      }
    }
  } 
}

int writeNodeConfig()
{
/*  DynamicJsonBuffer jsonBuffer(1000);
  JsonObject& root = jsonBuffer.createObject();
  root["useWifi"] = int(useWifi);
  root["NetBIOSName"] = NetBIOSName;
  root["useDHCP"] = int(useDHCP);
  root["statIP"] = statIP;
  root["statGateway"] = statGateway;
  root["statNetmask"] = statNetmask;
  root["statDNS"] = statDNS;

  root["useAP"] = int(useAP);
  root["apName"] = apName;
  root["apGateway"] = apGateway;
  root["apUser"] = apUser;
  root["apPassword"] = apPassword;

  root["useNTP"] = int(useNTP);
  root["NTPServer"] = ntpServer;
  root["ntpTimeZone"] = timeZone;
  root["actAsGateway"] = actAsGateway;
  root["useMQTTCmd"] = useMQTTCommand;

  JsonObject& data = root.createNestedObject("mqttServer");
  data["ip"] = mqtt_server;
  data["port"] = mqtt_port;
  data["user"] = mqtt_user;
  data["password"] = mqtt_password;

  root["lnBCTopic"] = lnBCTopic;
  root["lnEchoTopic"] = lnEchoTopic;
  
  String newMsg = "";
  root.printTo(newMsg);
*/
  String newMsg = handleConfigJSON();
  Serial.println(newMsg);
  Serial.println("Writing Node Config File");
  File dataFile = SPIFFS.open("/node.cfg", "w");
  if (dataFile)
  {
    dataFile.println(newMsg);
    dataFile.close();
    Serial.println("Writing Config File complete");
  }
}

uint32_t getColorByName(String colName)
{
//  Serial.print(colName.c_str());
//  Serial.println("#");
  for (int i=0; i<ledColorListSize;i++)
  {
    ledColorEntry * thisColor = &ledColorList[i];
//    Serial.print("compare to: ");
//    Serial.print(thisColor->colName);
//    Serial.println("#");
//    delay(1000);
    if(colName.equals(String(thisColor->colName)))
    {
      return ((thisColor->colRGB[0]<<16) + (thisColor->colRGB[1]<<8) + thisColor->colRGB[2]);
    }
  }
//  Serial.println("not found");
  return 0xFFFFFF;
}

//read node config file with variable settings
int readSignalConfig()
{
//  Serial.print("Call 1");
  DynamicJsonBuffer jsonBuffer(20000);
  if (SPIFFS.exists("/signals.cfg"))
  {
//  Serial.print("Call 2");
    File dataFile = SPIFFS.open("/signals.cfg", "r");
//  Serial.print("Call 3");
    if (dataFile)
    {
      String jsonData;
      while (dataFile.position()<dataFile.size())
      {
        jsonData = jsonData + dataFile.readStringUntil('\n');
        jsonData.trim();
      } 
      dataFile.close();
      Serial.print("Reading Signal Config File done");
      JsonObject& root = jsonBuffer.parseObject(jsonData);
      if (root.success())
      {
        if (root.containsKey("LEDCols"))
        {
          JsonArray& LEDCols = root["LEDCols"];
          ledColorList = (ledColorEntry*) realloc (ledColorList, LEDCols.size() * sizeof(ledColorEntry));
          ledColorListSize = LEDCols.size();
          for (int i=0; i<ledColorListSize;i++)
          {
            ledColorEntry * thisColor = &ledColorList[i];
            String colName = LEDCols[i]["Name"];
            strcpy(thisColor->colName,colName.c_str()); 
            thisColor->colRGB[0] = LEDCols[i]["RGBVal"][0];
            thisColor->colRGB[1] = LEDCols[i]["RGBVal"][1];
            thisColor->colRGB[2] = LEDCols[i]["RGBVal"][2];
            thisColor->blinkRate = LEDCols[i]["RGBVal"][3];
          }
        }

        if (root.containsKey("Brightness"))
        {
          globalBrightness = root["Brightness"]["PercentVal"];
        }
        if (root.containsKey("Fading"))
        {
          fadeCtrlSteps = root["Fading"]["PercentVal"];
        }

        Serial.println(String(ESP.getFreeHeap()));
        if (root.containsKey("SigHeads"))
        {
          JsonArray& sigHeads = root["SigHeads"];

          //allocate memory for main signal object list
          signalHeadCtrlDefList = (signalHeadCtrlDef*) realloc (signalHeadCtrlDefList, sigHeads.size() * sizeof(signalHeadCtrlDef));
          signalHeadCtrlDefListSize = sigHeads.size();

          //calcualte list sizes for each support list type
          uint16_t sigAddrCtr = 0;
          uint16_t lampDefCtr = 0;
          uint16_t aspectCtr = 0;
          uint16_t aspectLampCtr = 0;
          for (int i=0; i<signalHeadCtrlDefListSize;i++)
          {
            JsonObject& jsonSigHead = sigHeads[i];
            JsonArray& sigAddr = jsonSigHead["SwitchAddr"];
            sigAddrCtr += sigAddr.size();
            JsonArray& lampDef = jsonSigHead["Display"]["Lamps"];
            lampDefCtr += lampDef.size();
            JsonArray& aspectDef = jsonSigHead["Aspects"];
            aspectCtr += aspectDef.size();
            aspectLampCtr += (lampDef.size() * aspectDef.size());
          }
          ledChainLength = lampDefCtr; //now we know how many LED's
          
          //allocate memory for lists
          sigAddressList = (uint16_t*) realloc (sigAddressList, sigAddrCtr * sizeof(uint16_t));
          sigAddressListSize = sigAddrCtr;
          lampDescriptorList = (lampDescriptor*) realloc (lampDescriptorList, lampDefCtr * sizeof(lampDescriptor));
          lampDescriptorListSize = lampDefCtr;
          aspectDescriptorList = (aspectDescriptor*) realloc (aspectDescriptorList, aspectCtr * sizeof(aspectDescriptor));
          aspectDescriptorListSize = aspectCtr;
          aspectLampDescriptorList = (aspectLampDescriptor*) realloc (aspectLampDescriptorList, aspectLampCtr * sizeof(aspectLampDescriptor));
          aspectLampDescriptorListSize = aspectLampCtr;
          
          //loadint objects into memory buffers
          sigAddrCtr = 0; //reset counters, now serving as index pointers
          lampDefCtr = 0;
          aspectCtr = 0;
          aspectLampCtr = 0;
          
          for (int i=0; i<signalHeadCtrlDefListSize;i++)
          {
            JsonObject& jsonSigHead = sigHeads[i];
            signalHeadCtrlDef * thisSigHead = &signalHeadCtrlDefList[i];
            thisSigHead->addrMode = jsonSigHead["AddrMode"];
            thisSigHead->transMode = jsonSigHead["TransMode"];
            thisSigHead->chgDelay = jsonSigHead["ChgDelay"];
            thisSigHead->currentAspect = 0;
            thisSigHead->chgStatus = 0;
            
            JsonArray& sigAddr = jsonSigHead["SwitchAddr"];
            thisSigHead->sigAddrListSize = sigAddr.size();
            thisSigHead->sigAddrListStart = sigAddrCtr;
            for (int j=0; j < sigAddr.size(); j++)
            {
              sigAddressList[sigAddrCtr] = sigAddr[j];
              sigAddrCtr++;
            }
            JsonArray& lampDefs = jsonSigHead["Display"]["Lamps"];
            thisSigHead->sigLampDescriptorListSize = lampDefs.size();
            thisSigHead->sigLampDescriptorListStart = lampDefCtr;
//            lampDefCtr += thisSigHead->lampDescriptorListSize;
            for (int j=0; j < lampDefs.size(); j++)
            {
              lampDescriptor * thisLamp = &lampDescriptorList[lampDefCtr];
              thisLamp->lampIndex = lampDefs[j]["Ind"];
              thisLamp->targetColor = CRGB::Black;
              thisLamp->oldTargetColor = CRGB::Black;
              thisLamp->adjTargetColor = CRGB::Black;
              thisLamp->blinkPeriod = 0;
              thisLamp->blinkTimer = millis();
              thisLamp->lampStatus = 0;
              lampDefCtr++;
            }
            JsonArray& aspectDefs = jsonSigHead["Aspects"];
            thisSigHead->sigAspectDescriptorListSize = aspectDefs.size();
            thisSigHead->sigAspectDescriptorListStart = aspectCtr;
            for (int j=0; j < aspectDefs.size(); j++)
            {
              aspectDescriptor * thisAspect = &aspectDescriptorList[aspectCtr];
              JsonArray& aspectIntVal = aspectDefs[j]["IntVal"];
              thisAspect->aspIntval[0] = aspectIntVal[0];
              thisAspect->aspIntval[1] = aspectIntVal[1];

              JsonArray& aspectLampDefs = aspectDefs[j]["Lamps"];
              thisAspect->aspectLampListStart = aspectLampCtr;
              thisAspect->aspectLampListSize = aspectLampDefs.size();
              for (int k=0; k < aspectLampDefs.size(); k++)
              {
                aspectLampDescriptor * thisAspectLamp = &aspectLampDescriptorList[aspectLampCtr];
                thisAspectLamp->lampColor = getColorByName(aspectLampDefs[k]["Color"]);
//                Serial.printf("Color %i loaded for Lamp %i in aspect %i of signal %i \n", thisAspectLamp->lampColor, k, j, i); 
                thisAspectLamp->blinkPeriod = aspectLampDefs[k]["Blink"];
                aspectLampCtr++;
              }
              Serial.printf("Loading aspect %i Lamp Start %i Lamps %i \n", j, thisAspect->aspectLampListStart, thisAspect->aspectLampListSize);
              aspectCtr++;
            }
          }
          Serial.printf("Allocating memory for %i signals, %i Addresses, %i Lamps, %i Aspects, %i AspectLampDefs \n", signalHeadCtrlDefListSize, sigAddrCtr, lampDefCtr, aspectCtr, aspectLampCtr);
        Serial.println(String(ESP.getFreeHeap()));
        }
      }
    }
  }
}


//==============================================================Web Server=================================================

void handleDisplayJSONRequests(JsonObject& root)
{
  if ((root.containsKey("LED")) && (root.containsKey("Color")))
  {
//    int * LEDList[] = root["LED"];
//    int * colList[] = root["Color"];
    int colVal = 0;
    if (root["Color"].size() >= 3)
    {
      for (int i =0; i < 3; i++)
      {
        byte newVal = root["Color"][i]; 
        colVal = (colVal * 256) + newVal;
      }
    }
    if (root["LED"].size() == 0)
      for (int i =0; i < ledChainLength; i++) 
        leds[i] = 0;
    else
      for (int i =0; i < root["LED"].size(); i++) 
      {
        byte ledNr = root["LED"][i];
        leds[ledNr] = colVal;
      }
    FastLED.show();   
  }
}
  
void handleDisplayTestJSONRequests(JsonObject& root)
{
  Serial.println("Test LED Chain");
  if ((root.containsKey("Num")) && (root.containsKey("Color")))
  {
    int colVal = 0;
    int numLeds = root["Num"];
    if (root["Color"].size() >= 3)
    {
      for (int i =0; i < 3; i++)
      {
        byte newVal = root["Color"][i]; 
        colVal = (colVal * 256) + newVal;
      }
    }
    for (int i =0; i < numLeds; i++) 
      leds[i] = colVal;
    FastLED.show();   
  }
}

void handleEmulatorJSONRequests(JsonObject& root)
{
  useEmulator = root["useEmulator"]; 
  if (!useEmulator)
  {
    refreshAll = true;
  }
  else
  {
    //clear all LEDs
    for (int i = 0; i < ledChainLength; i++)
      leds[i] = 0;
    FastLED.show();
  }
}

void handleConfigJSONRequests(JsonObject& root) 
{
  Serial.println("Starting Update");
  char hlpStr[50];
  bool hlpBool;
  int hlpInt;
  bool changedNodeData = false;
  bool needReboot = false;
  if (root.containsKey("useWifi"))
  {
    hlpBool = root["useWifi"];
    changedNodeData |= (useWifi != hlpBool);
    needReboot |= (useWifi != hlpBool);
    useWifi = hlpBool;
    Serial.printf("use Wifi: %b", useWifi);
  }
  if (root.containsKey("NetBIOSName"))
  {
    strcpy(hlpStr, root["NetBIOSName"]);
    changedNodeData |= (netBIOSName != hlpStr);
    needReboot |= (netBIOSName != hlpStr);
    strcpy(netBIOSName, hlpStr);
  }
  if (root.containsKey("useDHCP"))
  {
    hlpBool = root["useDHCP"];
    changedNodeData |= (useDHCP != hlpBool);
    needReboot |= (useDHCP != hlpBool);
    useDHCP = hlpBool;
  }
  if (root.containsKey("staticIP"))
  {
    strcpy(hlpStr, root["staticIP"]);
    changedNodeData |= (staticIP != hlpStr);
    needReboot |= (staticIP != hlpStr);
    strcpy(staticIP, hlpStr);
  }
  if (root.containsKey("staticGateway"))
  {
    strcpy(hlpStr, root["staticGateway"]);
    changedNodeData |= (staticGateway != hlpStr);
    needReboot |= (staticGateway != hlpStr);
    strcpy(staticGateway, hlpStr);
  }
  if (root.containsKey("staticNetmask"))
  {
    strcpy(hlpStr, root["staticNetmask"]);
    changedNodeData |= (staticNetmask != hlpStr);
    needReboot |= (staticNetmask != hlpStr);
    strcpy(staticNetmask, hlpStr);
  }
  if (root.containsKey("staticDNS"))
  {
    strcpy(hlpStr, root["staticDNS"]);
    changedNodeData |= (staticDNS != hlpStr);
    needReboot |= (staticDNS != hlpStr);
    strcpy(staticDNS, hlpStr);
  }

  if (root.containsKey("useAP"))
  {
    hlpBool = root["useAP"];
    changedNodeData |= (useAP != hlpBool);
    needReboot |= (useAP != hlpBool);
    useAP = hlpBool;
  }
  if (root.containsKey("apName"))
  {
    strcpy(hlpStr, root["apName"]);
    changedNodeData |= (apName != hlpStr);
    needReboot |= (apName != hlpStr);
    strcpy(apName, hlpStr);
  }
  if (root.containsKey("apGateway"))
  {
    strcpy(hlpStr, root["apGateway"]);
    changedNodeData |= (apGateway != hlpStr);
    needReboot |= (apGateway != hlpStr);
    strcpy(apGateway, hlpStr);
  }
//  if (root.containsKey("apUser"))
//  {
//    strcpy(hlpStr, root["apUser"]);
//    changedNodeData |= (apUser != hlpStr);
//    needReboot |= (apUser != hlpStr);
//    strcpy(apUser, hlpStr);
//  }
  if (root.containsKey("apPassword"))
  {
    strcpy(hlpStr, root["apPassword"]);
    changedNodeData |= (apPassword != hlpStr);
    needReboot |= (apPassword != hlpStr);
    strcpy(apPassword, hlpStr);
  }
  if (root.containsKey("useNTP"))
  {
    hlpBool = root["useNTP"];
    changedNodeData |= (useNTP != hlpBool);
    useNTP = hlpBool;
  }
  if (root.containsKey("ntpTimeZone"))
  {
    int hlpInt = root["ntpTimeZone"];
    changedNodeData |= (timeZone != hlpInt);
    timeZone = hlpInt;
  }
  if (root.containsKey("NTPServer"))
  {
    strcpy(hlpStr, root["NTPServer"]);
    changedNodeData |= (ntpServer != hlpStr);
    strcpy(ntpServer, hlpStr);
  }
  if (root.containsKey("useBushby"))
  {
    hlpBool = root["useBushby"];
    changedNodeData |= (useBushby != hlpBool);
    useBushby = hlpBool;
    mqttClient.disconnect();
  }
  if (root.containsKey("useInputMode"))
  {
    hlpInt = root["useInputMode"];
    changedNodeData |= (useInputMode != hlpInt);
    useInputMode = hlpInt;
    mqttClient.disconnect();
  }
  if (root.containsKey("mqttServer"))
  {
    const char * hlpStr = root["mqttServer"]["ip"];
    if (hlpStr)
    {
      changedNodeData |= (mqtt_server != hlpStr);
      strcpy(mqtt_server, hlpStr);
      mqttClient.disconnect();
    }
    hlpStr = root["mqttServer"]["port"];
    if (hlpStr)
    {
      int hlpInt = root["mqttServer"]["port"];
      changedNodeData |= (mqtt_port != hlpInt);
      mqtt_port = hlpInt;
      mqttClient.disconnect();
    }
    hlpStr = root["mqttServer"]["user"];
    if (hlpStr)
    {
      changedNodeData |= (mqtt_user != hlpStr);
      strcpy(mqtt_user, hlpStr);
      mqttClient.disconnect();
    }
    hlpStr = root["mqttServer"]["password"];
    if (hlpStr)
    {
      changedNodeData |= (mqtt_password != hlpStr);
      strcpy(mqtt_password, hlpStr);
      mqttClient.disconnect();
    } 
  }
  if (root.containsKey("lnBCTopic"))
  {
    strcpy(hlpStr, root["lnBCTopic"]);
    changedNodeData |= (lnBCTopic != hlpStr);
    strcpy(lnBCTopic, hlpStr);
    mqttClient.disconnect();
  }
  if (root.containsKey("lnEchoTopic"))
  {
    strcpy(hlpStr, root["lnEchoTopic"]);
    changedNodeData |= (lnEchoTopic != hlpStr);
    strcpy(lnEchoTopic, hlpStr);
    mqttClient.disconnect();
  }
  if (root.containsKey("ResetCtr"))
  {
    bytesReceived = 0;
    bytesTransmitted = 0;
    bytesLastReceived = 0;
  }
  if (changedNodeData)
  {
    writeNodeConfig();
    delay(500);
  }
  if (needReboot | (root.containsKey("RebootNow")))
    ESP.restart();
}

String handleConfigJSON()
{
  String response;
  float float1;
  long curTime = now();
  DynamicJsonBuffer jsonBuffer(1500);
  JsonObject& root = jsonBuffer.createObject();

  //config data stuff
  root["useWifi"] = int(useWifi);
  root["NetBIOSName"] = netBIOSName;
  root["useDHCP"] = int(useDHCP);
  root["staticIP"] = staticIP;
  root["staticGateway"] = staticGateway;
  root["staticNetmask"] = staticNetmask;
  root["staticDNS"] = staticDNS;

  root["useAP"] = int(useAP);
  root["apName"] = apName;
  root["apGateway"] = apGateway;
  root["apPassword"] = apPassword;

  root["useNTP"] = int(useNTP);
  root["NTPServer"] = ntpServer;
  root["ntpTimeZone"] = timeZone;
  root["useBushby"] = int(useBushby);
  root["useInputMode"] = useInputMode;

  JsonObject& data = root.createNestedObject("mqttServer");
  data["ip"] = mqtt_server;
  data["port"] = mqtt_port;
  data["user"] = mqtt_user;
  data["password"] = mqtt_password;

  root["lnBCTopic"] = lnBCTopic;
  root["lnEchoTopic"] = lnEchoTopic;
  
  root.printTo(response);
  Serial.println(response);
  return response;
}

String handleRuntimeJSON()
{
  String response;
  float float1;
  long curTime = now();
  DynamicJsonBuffer jsonBuffer(1500);
  JsonObject& root = jsonBuffer.createObject();

    //read only statistics stuff
    if (WiFi.status() == WL_CONNECTED)
    {
      root["IP"] = WiFi.localIP().toString();
      long rssi = WiFi.RSSI();
      root["SigStrength"] = rssi;
    }
    root["SWVersion"] = swVersion;
    float1 = (millisRollOver * 4294967.296) + millis()/1000;
    root["uptime"] = round(float1);
    if (ntpOK && useNTP)
    {
      if (NTPch.daylightSavingTime(curTime))
        curTime -= (3600 * (timeZone+1));
      else
        curTime -= (3600 * timeZone);
      root["currenttime"] = curTime;  //seconds since 1/1/1970
    }
    root["mem"] = ESP.getFreeHeap();
    root["BytesReceived"] = bytesReceived;
    root["BytesTransmitted"] = bytesTransmitted;
    root["LNLoadBps"] = networkLoad.average();
    root["LNLoad100"] = 100 * networkLoad.average() / (lnLoadIntervall / 0.6);
  root.printTo(response);
  Serial.println(response);
  return response;
}

void handleSignalJSONRequests(JsonObject& root) 
{
  
}

String handleSignalJSON(bool inclTransients)
{
  String response = "{\"test\":0}";  
  return response;
}


String handlePingJSON()
{
  String response;
  float float1;
  long curTime = now();
  DynamicJsonBuffer jsonBuffer(500);
  JsonObject& root = jsonBuffer.createObject();
  root["IP"] = WiFi.localIP().toString();
  if (WiFi.status() == WL_CONNECTED)
  {
    long rssi = WiFi.RSSI();
    root["SigStrength"] = rssi;
  }
  root["NetBIOSName"] = netBIOSName;
    root["mem"] = ESP.getFreeHeap();
  float1 = (millisRollOver * 4294967.296) + millis()/1000;
  root["uptime"] = round(float1);
  root["time"] = curTime;
  root.printTo(response);
  Serial.println(response);
  return response;
}
