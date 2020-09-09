/*
   Captive Portal by: M. Ray Burnette 20150831
   See Notes tab for original code references and compile requirements
   Sketch uses 300,640 bytes (69%) of program storage space. Maximum is 434,160 bytes.
   Global variables use 50,732 bytes (61%) of dynamic memory, leaving 31,336 bytes for local variables. Maximum is 81,920 bytes.
*/
#define STATION 1
// #define SOFTAP 1

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <Ticker.h>
#endif
#if defined(STATION)
#include "./MySSID.h"
#elif defined(SOFTAP)
#include "./DNSServer.h"                  // Patched lib
// Capture DNS requests on port 53
IPAddress         apIP(10, 10, 10, 1);    // Private network for server
DNSServer         dnsServer;              // Create the DNS object

#endif
const int RELAYS = 8;
// An array of charging time indexed by the measured voltage. Allows a flat battery to be charged longer and a fully charged battery to be charged for a short time
// Volts                     0, 1,  2,  3,  4,  5,  6,  7,  8,    9,    10,   11,   12, 13, 14, 15, 16
const int CHARGEMINUTES[] = {1, 1,  1,  1,  1,  30, 30, 30, 240,  240,  240,  100,  30, 5,  1,  1,  1};

const byte        DNS_PORT = 53; 

Ticker logger;
Ticker minutes;


#if defined(ESP8266)
ESP8266WebServer  webServer(80);          // HTTP server


#elif defined(ESP32)
WebServer  webServer(80); 
#endif

// Wemos pin definitions Batteries 1 - 8
#if defined(ESP8266)
int gpioPin[] = { 16,5,4,14,12,13,0,2 }; 
const int sensorPin = A0;
const float CALIBRATION = 0.014273256; // 8.2/2.2 ohm resistor divider (3.313 / .694V = 234 of 1024 )
#elif defined(ESP32)
int gpioPin[] = { 26,25,17,16,27,14,12,13 }; 

const float CALIBRATION = 0.004042956; // 8.2/2.2 ohm resistor divider (5V / 1/074V = 1128 of 4096 )
const int sensorPin = 34;
#endif

float batVolts[RELAYS];
int charged[RELAYS];
int currentCharger = -1 ;                   
int sensorValue = 0;
int firstBoot = 1;
int lastMinutes,lastHour,lastDay;
int32_t last_global_millis = 0;
int32_t new_global_millis = 0;
int startMinutes = 0; 
int startHours = 0;
int startDays = 0;


void logbat (){
  int t = analogRead(sensorPin);
  String bat = "Selected battery ";
  startMinutes++;
  if ( ! (startMinutes % 60) ){
    startHours++;
  } 
  if ( ! (startMinutes % 1440) ){
    startDays++;
  }
  String theTime = "Current Time: ";
  theTime = theTime + " " + startMinutes + " minutes " + startHours + " hours " + startDays + " days ";
  int num = currentCharger + 1;
  bat = theTime + " " + bat + num + " " + t + " " + String(t * CALIBRATION, 2);
  Serial.println(bat);
}

void setup() {
  Serial.begin(115200);
  logger.attach(60,logbat);
#if defined(SOFTAP)
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("BatteryChargerE32");
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);
#elif defined(STATION)
  WiFi.mode(WIFI_STA);
  WiFi.begin(mySSID,myPSK);
  Serial.println("");
    // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(mySSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif
  

  // replay to all requests with same HTML
  webServer.onNotFound([]() {

    String headerHTML =   "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"10\">\
                      <title>Battery Charger</title></head><body>\
                      <style>\
                      table {\
  border-collapse: collapse;\
  width: 60%;\
  font-size: 50px;\
}\
\
table, th, td {\
  border: 1px solid black;\
}\
</style>";

    String bodyHTML =  "<H1> Battery Charger </H1>";
    String timeSinceBoot = "Time since boot ";
    timeSinceBoot = timeSinceBoot + " " + startMinutes + " minutes " + startHours + " hours " + startDays + " days<BR>";
    bodyHTML = bodyHTML + timeSinceBoot;
    // bodyHTML = bodyHTML + CHARGEMINUTES[(int)batVolts[currentCharger]] + " minutes/battery </H1>";

    String footerHTML =   "</body></html>";
    
    int minutes = CHARGEMINUTES[(int)batVolts[currentCharger]] - ( startMinutes - lastMinutes ) ; // Calculate charging time on this battery
    String batString = "<TABLE><TR><TH>Battery</TH><TH>Volts</TH><TH>Minutes</TH>\n";
   
    int j;
    for (int i = 0; i <RELAYS; i++){
      j=i+1;
      if ( i == currentCharger )
        batString = batString + "<TR><TD style=\"background-color:Tomato;\">" + j + "</TD><TD>"+ String(batVolts[i], 2) + "<TD>" + CHARGEMINUTES[(int)batVolts[i]] + " (" + minutes +  ")</TD></TR>\n";
      else batString = batString + "<TR><TD>" + j + "</TD><TD>"+ String(batVolts[i], 2)+ " </TD><TD>"+ CHARGEMINUTES[(int)batVolts[i]] + "</TD></TR>\n";
    }
    batString = batString + "</TABLE>";
    String responseHTML = headerHTML + bodyHTML + batString + footerHTML;
    webServer.send(200, "text/html", responseHTML);
  });
  webServer.begin();
  for(int i = 0; i < RELAYS; i++) {
    pinMode(gpioPin[i], OUTPUT);
    digitalWrite(gpioPin[i], HIGH);
  }
  lastMinutes = startMinutes;
}

int pickbattery(int num) {
  for(int i = 0; i < RELAYS; i++) {
     digitalWrite(gpioPin[i], HIGH);
  }
  digitalWrite(gpioPin[num], LOW);
}


void loop() {
#if defined(SOFTAP)
  dnsServer.processNextRequest();
#endif
  webServer.handleClient(); 
  batVolts[currentCharger] = analogRead(sensorPin) * CALIBRATION;
  // See if we have rolled over, delayed enough or current battery has < 1.2V
  if (( startMinutes > (lastMinutes + CHARGEMINUTES[(int)batVolts[currentCharger]]))
  ||( startMinutes < lastMinutes) 
  || firstBoot ) {
    lastMinutes = startMinutes;
    if (currentCharger >= RELAYS-1 ) {
      currentCharger = -1;
      firstBoot = 0;
    }
      
      
      pickbattery(++currentCharger);
  }
  //Serial.println(responseHTML);
  
  delay(500);
  
}
