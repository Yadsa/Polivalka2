/***************************************************************************************************************
 * IoT DHT Temp/Hum Station using NodeMCU ESP-12 Develop Kit V1.0
 *  DHT connected to NodeMCU pin D3
 *  DHT Data on local OLED Display
 *
 *  Developed by MJRovai 12 October 2017
 ********************************************************************************************************************************/

/* OLED */
#include <ACROBOTIC_SSD1306.h> // library for OLED: SCL ==> D1; SDA ==> D2
#include <SPI.h>
#include <Wire.h>

/* ESP & Blynk */
//#define BLYNK_DEBUG // Optional, this enables lots of prints
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <functional>

/* DHT22*/
#include "DHT.h"

#include "stationDefines.h"
#include "stationCredentials.h"

DHT dht(DHTPIN, DHTTYPE);
float hum = 0;
float temp = 0;
long distance = 0;
bool action = 0;
bool onoff = 1;
int prevdisplaystate = 0;
int displayon=1;
int lightenough=0;

void prepareIds();
boolean connectWifi();
boolean connectUDP();
void startHttpServer();
void turnOnRelay();
void turnOffRelay();

//const char* ssid = "Aruna";
//const char* password = "********";

unsigned int localPort = 1900;      // local port to listen on

WiFiUDP UDP;
boolean udpConnected = false;
IPAddress ipMulti(239, 255, 255, 250);
unsigned int portMulti = 1900;      // local port to listen on

ESP8266WebServer HTTP(80);
 
boolean wifiConnected = false;

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

String serial;
String persistent_uuid;
String device_name;

const int relayPin = D9;

boolean cannotConnectToWifi = false;


/* TIMER */
#include <SimpleTimer.h>
SimpleTimer timer;


void setup() 
{
  Serial.begin(115200);
  delay(10);
  Serial.println(".....");
  Serial.println("Polivalka 2");
  Serial.println("Connecting to Blynk");
  Serial.println("..");
  Serial.println(".");

  pinMode(PROXIMITY_TRIGGER, OUTPUT);
  pinMode(PROXIMITY_ECHO, INPUT);

  //Blynk.begin(auth, ssid, pass);

  pinMode(D8, INPUT);

  prepareIds();
  
  // Initialise wifi connection
  wifiConnected = connectWifi();

  // only proceed if wifi connection successful
  if(wifiConnected){
    udpConnected = connectUDP();
    
    if (udpConnected){
      // initialise pins if needed 
      startHttpServer();
    }
  }  
  
  dht.begin();
  oledStart();
  radiosetup();
  startTimers();
}

void prepareIds() {
  uint32_t chipId = ESP.getChipId();
  char uuid[64];
  sprintf_P(uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
        (uint16_t) ((chipId >> 16) & 0xff),
        (uint16_t) ((chipId >>  8) & 0xff),
        (uint16_t)   chipId        & 0xff);

  serial = String(uuid);
  persistent_uuid = "Socket-1_0-" + serial;
  device_name = "box";
}

void respondToSearch() {
    Serial.println("");
    Serial.print("Sending response to ");
    Serial.println(UDP.remoteIP());
    Serial.print("Port : ");
    Serial.println(UDP.remotePort());

    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    String response = 
         "HTTP/1.1 200 OK\r\n"
         "CACHE-CONTROL: max-age=86400\r\n"
         "DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
         "EXT:\r\n"
         "LOCATION: http://" + String(s) + ":80/setup.xml\r\n"
         "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
         "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
         "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
         "ST: urn:Belkin:device:**\r\n"
         "USN: uuid:" + persistent_uuid + "::urn:Belkin:device:**\r\n"
         "X-User-Agent: redsonic\r\n\r\n";

    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.write(response.c_str());
    UDP.endPacket();                    

     Serial.println("Response sent !");
}

void startHttpServer() {
    HTTP.on("/index.html", HTTP_GET, [](){
      Serial.println("Got Request index.html ...\n");
      HTTP.send(200, "text/plain", "Hello World!");
    });

    HTTP.on("/upnp/control/basicevent1", HTTP_POST, []() {
      Serial.println("########## Responding to  /upnp/control/basicevent1 ... ##########");      

      //for (int x=0; x <= HTTP.args(); x++) {
      //  Serial.println(HTTP.arg(x));
      //}
  
      String request = HTTP.arg(0);      
      Serial.print("request:");
      Serial.println(request);
 
      if(request.indexOf("<BinaryState>1</BinaryState>") > 0) {
          Serial.println("Got Turn on request");
          turnOnRelay();
      }

      if(request.indexOf("<BinaryState>0</BinaryState>") > 0) {
          Serial.println("Got Turn off request");
          turnOffRelay();
      }
      
      HTTP.send(200, "text/plain", "");
    });

    HTTP.on("/eventservice.xml", HTTP_GET, [](){
      Serial.println(" ########## Responding to eventservice.xml ... ########\n");
      String eventservice_xml = "<?scpd xmlns=\"urn:Belkin:service-1-0\"?>"
            "<actionList>"
              "<action>"
                "<name>SetBinaryState</name>"
                "<argumentList>"
                  "<argument>"
                    "<retval/>"
                    "<name>BinaryState</name>"
                    "<relatedStateVariable>BinaryState</relatedStateVariable>"
                    "<direction>in</direction>"
                  "</argument>"
                "</argumentList>"
                 "<serviceStateTable>"
                  "<stateVariable sendEvents=\"yes\">"
                    "<name>BinaryState</name>"
                    "<dataType>Boolean</dataType>"
                    "<defaultValue>0</defaultValue>"
                  "</stateVariable>"
                  "<stateVariable sendEvents=\"yes\">"
                    "<name>level</name>"
                    "<dataType>string</dataType>"
                    "<defaultValue>0</defaultValue>"
                  "</stateVariable>"
                "</serviceStateTable>"
              "</action>"
            "</scpd>\r\n"
            "\r\n";
            
      HTTP.send(200, "text/plain", eventservice_xml.c_str());
    });
    
    HTTP.on("/setup.xml", HTTP_GET, [](){
      Serial.println(" ########## Responding to setup.xml ... ########\n");

      IPAddress localIP = WiFi.localIP();
      char s[16];
      sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    
      String setup_xml = "<?xml version=\"1.0\"?>"
            "<root>"
             "<device>"
                "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
                "<friendlyName>"+ device_name +"</friendlyName>"
                "<manufacturer>Belkin International Inc.</manufacturer>"
                "<modelName>Emulated Socket</modelName>"
                "<modelNumber>3.1415</modelNumber>"
                "<UDN>uuid:"+ persistent_uuid +"</UDN>"
                "<serialNumber>221517K0101769</serialNumber>"
                "<binaryState>0</binaryState>"
                "<serviceList>"
                  "<service>"
                      "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
                      "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
                      "<controlURL>/upnp/control/basicevent1</controlURL>"
                      "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                      "<SCPDURL>/eventservice.xml</SCPDURL>"
                  "</service>"
              "</serviceList>" 
              "</device>"
            "</root>\r\n"
            "\r\n";
            
        HTTP.send(200, "text/xml", setup_xml.c_str());
        
        Serial.print("Sending :");
        Serial.println(setup_xml);
    });
    
    HTTP.begin();  
    Serial.println("HTTP Server started ..");
}


      
// connect to wifi – returns true if successful or false if not
boolean connectWifi(){
  boolean state = true;
  int i = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting ...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 10){
      state = false;
      break;
    }
    i++;
  }
  
  if (state){
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("");
    Serial.println("Connection failed.");
  }
  
  return state;
}

boolean connectUDP(){
  boolean state = false;
  
  Serial.println("");
  Serial.println("Connecting to UDP");
  
  if(UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti)) {
    Serial.println("Connection successful");
    state = true;
  }
  else{
    Serial.println("Connection failed");
  }
  
  return state;
}

void turnOnRelay() {
 digitalWrite(relayPin, HIGH); // turn on relay with voltage HIGH 
}

void turnOffRelay() {
  digitalWrite(relayPin, LOW);  // turn off relay with voltage LOW
}

void loop() 
{
 // timer.run();
  HTTP.handleClient();
  delay(1);
  
  
  // if there's data available, read a packet
  // check if the WiFi and UDP connections were successful
  if(wifiConnected){
    if(udpConnected){    
      // if there’s data available, read a packet
      int packetSize = UDP.parsePacket();
      
      if(packetSize) {
        Serial.println("");
        Serial.print("Received packet of size ");
        Serial.println(packetSize);
        Serial.print("From ");
        IPAddress remote = UDP.remoteIP();
        
        for (int i =0; i < 4; i++) {
          Serial.print(remote[i], DEC);
          if (i < 3) {
            Serial.print(".");
          }
        }
        
        Serial.print(", port ");
        Serial.println(UDP.remotePort());
        
        int len = UDP.read(packetBuffer, 255);
        
        if (len > 0) {
            packetBuffer[len] = 0;
        }

        String request = packetBuffer;
        //Serial.println("Request:");
        //Serial.println(request);
         
        if(request.indexOf('M-SEARCH') > 0) {
            if(request.indexOf("urn:Belkin:device:**") > 0) {
                Serial.println("Responding to search request ...");
                respondToSearch();
            }
        }
      }
        
      delay(10);
    }
  } else {
      // Turn on/off to indicate cannot connect ..      
  }
  Blynk.run();
}

void onTimer()
{
  getDhtData();
  getProximityData();
  getLightData();
  checkShowDisplay();
  displayData();
}

void startTimers()
{
  timer.setInterval(2000, onTimer);
  timer.setInterval(2000, sendBlynkData);
  timer.setInterval(10000, afterShowDataOnce);
}
/***************************************************
 * Start OLED
 **************************************************/
void oledStart(void)
{
  Wire.begin();  
  oled.init();                      // Initialze SSD1306 OLED display
  clearOledDisplay();
  oled.clearDisplay();              // Clear screen
  oled.setTextXY(0,0);              
  oled.putString("  POLIVALKA 2");
}

void getLightData()
{
  lightenough = !digitalRead(D8);
}

void getProximityData()
{
  long duration;
  digitalWrite(PROXIMITY_TRIGGER, LOW);  
  delayMicroseconds(2); 
  
  digitalWrite(PROXIMITY_TRIGGER, HIGH);
  delayMicroseconds(10); 
  
  digitalWrite(PROXIMITY_TRIGGER, LOW);
  duration = pulseIn(PROXIMITY_ECHO, HIGH);
  distance = (duration/2) / 29.1;
  
  Serial.print(" Centimeter: ");
  Serial.println(distance);
}

/***************************************************
 * Get DHT data
 **************************************************/
void getDhtData(void)
{
  float tempIni = temp;
  float humIni = hum;
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  if (isnan(hum) || isnan(temp))   // Check if any reads failed and exit early (to try again).
  {
    Serial.println("Failed to read from DHT sensor!");
    temp = tempIni;
    hum = humIni;
    return;
  }
}

/***************************************************
 * Display data at Serial Monitora & OLED Display
 **************************************************/
void displayData(void)
{
  Serial.print(" Temperature: ");
  Serial.print(temp);
  Serial.print("oC   Humidity: ");
  Serial.print(hum);
  Serial.println("%");

  if (!displayon)
  {
    clearOledDisplay();
    simulate_button(1, 3, 0);
  }
  else
  {
    if (distance > 50)
    {
      oled.setTextXY(0,0);              
      oled.putString("  POLIVALKA 2");
      oled.setTextXY(2,0);
      oled.putString("To get temperature");
      oled.setTextXY(3,0);
      oled.putString("and humidity      ");
      oled.setTextXY(4,0);
      oled.putString("move closer.      ");
      oled.setTextXY(5,0);
      oled.putString("                  ");
      if (lightenough) {simulate_button(1, 3, 0);}
    } 
    else 
    {
      oled.setTextXY(0,0);              
      oled.putString("  POLIVALKA 2");
      oled.setTextXY(2,0);              // Set cursor position, start of line 2
      oled.putString("TEMP: " + String(temp) + " oC   ");
      oled.setTextXY(3,0);              // Set cursor position, start of line 2
      oled.putString("HUM:  " + String(hum) + " %   ");
      oled.setTextXY(4,0);              // Set cursor position, start of line 2
      oled.putString("DIST: " + String(distance) + " cm    ");
      oled.setTextXY(5,0);              // Set cursor position, start of line 2
      oled.putString("Action: " + String(action) + "       ");
      oled.setTextXY(6,0);              // Set cursor position, start of line 2
      oled.putString("OnOff: " + String(onoff) + "       ");
      oled.setTextXY(7,0);              // Set cursor position, start of line 2
      oled.putString("PDS: " + String(prevdisplaystate) + "       ");
      if (!lightenough) {simulate_button(1, 3, 1);}
    }
  }
}

/***************************************************
 * Clear OLED Display
 **************************************************/
void clearOledDisplay()
{
  oled.setFont(font8x8);
  oled.setTextXY(0,0); oled.putString("                ");
  oled.setTextXY(1,0); oled.putString("                ");
  oled.setTextXY(2,0); oled.putString("                ");
  oled.setTextXY(3,0); oled.putString("                ");
  oled.setTextXY(4,0); oled.putString("                ");
  oled.setTextXY(5,0); oled.putString("                ");
  oled.setTextXY(6,0); oled.putString("                ");
  oled.setTextXY(7,0); oled.putString("                ");
  oled.setTextXY(0,0); oled.putString("                ");              
}

/****************************************************************
* Read remote commands 
****************************************************************/
BLYNK_WRITE(10) // Pump remote control
{
  int i = param.asInt();
  if (i == 1) 
  {
    action = !action;
    displayon = 1;
    prevdisplaystate = 0;
  }
}

BLYNK_WRITE(11) // Lamp remote control
{
  int i = param.asInt();
  onoff = i;
  displayon = onoff;
  prevdisplaystate=0;
}

void sendBlynkData()
{
  Blynk.virtualWrite(0, temp); //virtual pin V0
  Blynk.virtualWrite(1, hum); // virtual pin V1
  Blynk.virtualWrite(2, distance); // virtual pin V2
  Blynk.virtualWrite(3, distance); // virtual pin V3
  Blynk.virtualWrite(11, onoff); // virtual pin V3
}

void afterShowDataOnce()
{
  if (distance > 30) 
  {
    //prevdisplaystate ++;
  }
}

void checkShowDisplay()
{
  if (distance > 30) {
    prevdisplaystate ++;
  } else {
    prevdisplaystate = 0;
    displayon=1;
  }
  
  if (prevdisplaystate >= 5)
  {
    displayon = 0; 
  }
}

void radiosetup()
{
  // Plug the TX module into A3-A5, with the antenna pin hanging off the end of the header.
  //pinMode(GND_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  //pinMode(VCC_PIN, OUTPUT);
  //pinMode(LED_PIN, OUTPUT);
  
  Serial.begin(115200);
}

void sendData(long payload1, long payload2)
{
  // Turn on the radio. A3=GND, A5=Vcc, A4=data)
  //digitalWrite(GND_PIN, LOW);
  //digitalWrite(VCC_PIN, HIGH);
  digitalWrite(DATA_PIN, HIGH);
  //digitalWrite(13, HIGH);
  
  // Send a preamble of 13 ms low pulse
  digitalWrite(DATA_PIN, LOW);
  for (int ii = 0; ii < 26; ii++)
  {
    delayMicroseconds(PULSE_WIDTH_SMALL);
  }
  digitalWrite(13, LOW);
  
  // send sync pulse : high for 0.5 ms
  digitalWrite(DATA_PIN, HIGH);
  delayMicroseconds(PULSE_WIDTH_SMALL);
  digitalWrite(DATA_PIN, LOW);
  
  // Now send the digits.  
  // We send a 1 as a state change for 1.5ms, and a 0 as a state change for 0.5ms
  long mask = 1;
  char state = HIGH;
  long payload = payload1;
  for (int jj = 0; jj < PAYLOAD_SIZE; jj++)
  {
    if (jj == 32)
    {
      payload = payload2;
      mask = 1;
    }
    
    char bit = (payload & mask) ? 1 : 0;
    mask <<= 1;
      
    state = !state;
    digitalWrite(DATA_PIN, state);
  
    delayMicroseconds(PULSE_WIDTH_SMALL);  
    if (bit)
    {
      delayMicroseconds(PULSE_WIDTH_SMALL);  
      delayMicroseconds(PULSE_WIDTH_SMALL);  
    }
  }
}

void simulate_button(int channel, int button, int on)
{
  long payload1 = buttons[(channel - 1) * 4 + (button - 1)];
  long payload2 = on? 13107L : 21299L;
  
  Serial.println(payload1);
  Serial.println(payload2);

  // Send the data 6 times
  for (int ii = 0; ii < 6; ii++)
  {
    sendData(payload1, payload2);
  }
  
  // turn off the radio
//  digitalWrite(VCC_PIN, LOW);
}


