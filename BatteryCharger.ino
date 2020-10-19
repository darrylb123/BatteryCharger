/*
   Captive Portal by: M. Ray Burnette 20150831
   See Notes tab for original code references and compile requirements
   Sketch uses 300,640 bytes (69%) of program storage space. Maximum is 434,160 bytes.
   Global variables use 50,732 bytes (61%) of dynamic memory, leaving 31,336 bytes for local variables. Maximum is 81,920 bytes.
*/
//#define STATION 1
#define SOFTAP 1
#include <stdio.h>
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

#if defined(SOFTAP)
#include "./DNSServer.h"                  // Patched lib
// Capture DNS requests on port 53
IPAddress         apIP(10, 10, 10, 1);    // Private network for server
DNSServer         dnsServer;              // Create the DNS object

#endif
const int RELAYS = 8;
// An array of charging time indexed by the measured voltage. Allows a flat battery to be charged longer and a fully charged battery to be charged for a short time
// Volts                     0, 1,  2,  3,  4,  5,  6,  7,  8,    9,    10,   11,   12, 13, 14, 15, 16
const int CHARGEMINUTES[] = {1, 1,  1,  1,  1,  30, 30, 30, 240,  240,  240,  100,  30, 5,  1,  1,  1};
const float fullyCharged = 11.9;
const byte        DNS_PORT = 53; 
String inputString;
int stringComplete;

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
int bankPin[] = {15,15,15,15,15,15,15,15 }; // Relay bank enable pin
const int sensorPin = A0;
const float CALIBRATION = 0.014273256; // 8.2/2.2 ohm resistor divider (3.313 / .694V = 234 of 1024 )
const char sapString[] = "Battery Charger";

#elif defined(ESP32)
int gpioPin[] = { 26,25,17,16,27,14,12,13 };
int bankPin[] = {5,5,5,5,5,5,5,5 }; // Relay bank enable pin
const char sapString[] = "Battery ChargerE32";
const float CALIBRATION = 0.004042956; // 8.2/2.2 ohm resistor divider (5V / 1/074V = 1128 of 4096 )
const int sensorPin = 34;
#endif

float batVolts[RELAYS];
int batteryCharged[RELAYS];
int batteryConnected[RELAYS];
int allCharged = 0;
int currentCharger = 0 ;                   
int sensorValue = 0;
int firstBoot = 1;
int lastMinutes,lastHour,lastDay;
int runMinutes = 0; 
int runHours = 0;
int runDays = 0;
char *responseHTML;

void logbat (){
  int t = analogRead(sensorPin);
  String bat = "Selected battery ";
  runMinutes++;
  if ( ! (runMinutes % 60) ){
    runHours++;
  } 
  if ( ! (runMinutes % 1440) ){
    runDays++;
  }
  String theTime = "Run Time: ";
  theTime = theTime + " " + runMinutes + " minutes " + runHours + " hours " + runDays + " days ";
  int num = currentCharger ;
  bat = theTime + " " + bat + num + " " + t + " " + String(t * CALIBRATION, 2);
  Serial.println(bat);
}
//Read a string from USB
void serialEvent() { 
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    //Serial.print(inChar);
    
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      // Serial.println("String Complete");
      stringComplete = true;
    } else {
      inputString += inChar;
    }
  }
}

// Web Server page handler
void handlePage (){
  char tempstr[200];

  strcpy(responseHTML,"<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"10\">\
                      <title>Battery Charger</title></head><body>\
                      <style>\
                      table {\
  border-collapse: collapse;\
  width: 100%;\
  font-size: 50px;\
}\
\
table, th, td {\
  border: 1px solid black;\
}\
</style>\
<H1> Battery Charger </H1>");
  
  sprintf(tempstr,"<H3>Time since boot %d minutes %d hours %d days</H3>\n",runMinutes,runHours,runDays);
  strcat(responseHTML,tempstr);
  
  strcat(responseHTML,"<TABLE><TR><TH>Battery</TH><TH>Volts</TH><TH>Minutes</TH><TH>Flags</TH>\n");

  int minutes = CHARGEMINUTES[(int)batVolts[currentCharger]] - ( runMinutes - lastMinutes ) ; // Calculate charging time on this battery
  for (int i = 0; i <RELAYS; i++){
    
    if ( i == currentCharger && !allCharged) {
      sprintf(tempstr,"<TR><TD style=\"background-color:Tomato;\"> %d </TD><TD> %5.2f </TD> <TD> %d ( %d ) </TD><TD> %d %d </TD><TR>\n",
          i+1,batVolts[i],CHARGEMINUTES[(int)batVolts[i]],minutes,batteryCharged[i],batteryConnected[i]);
    } else {
      sprintf(tempstr,"<TR><TD> %d </TD><TD> %5.2f </TD> <TD> %d  </TD><TD> %d %d </TD><TR>\n",
          i+1, batVolts[i],CHARGEMINUTES[(int)batVolts[i]],batteryCharged[i],batteryConnected[i]);
    }
    strcat(responseHTML,tempstr);
  }
  strcat(responseHTML,"</TABLE>");
  if (allCharged) {
    sprintf(tempstr,"<H3>All Charged, delaying next charge cycle until needed, scan in %d minutes<h3>\n",(60 -(runMinutes % 60)));
    strcat(responseHTML,tempstr);
  }
  strcat(responseHTML,"</body></html>\n");
  
    
  webServer.send(200, "text/html", responseHTML);
  // Serial.print(responseHTML);
}

// Configure wifi using ESP Smartconfig app on phone
int mySmartConfig() {
      WiFi.beginSmartConfig();

    //Wait for SmartConfig packet from mobile
    Serial.println("Waiting for SmartConfig.");
    while (!WiFi.smartConfigDone()) {
      delay(500);
      Serial.print(".");
    }

    Serial.println("");
    Serial.println("SmartConfig received.");

    //Wait for WiFi to connect to AP
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
}

void setup() {
  Serial.begin(115200);
  logger.attach(60,logbat);
  responseHTML = (char *)malloc(10000);
#if defined(SOFTAP)
  WiFi.mode(WIFI_AP);
  delay(2000);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(sapString);
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);
#elif defined(STATION)
  WiFi.mode(WIFI_STA);
    WiFi.begin();
  delay(5000);
  if (WiFi.status() != WL_CONNECTED) {
    mySmartConfig();
  }

  Serial.println("WiFi Connected.");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
 
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  delay(500);
#endif
  webServer.on("/",handlePage);
  webServer.onNotFound(handlePage);
  webServer.begin();
  
  for(int i = 0; i < RELAYS; i++) {
    pinMode(gpioPin[i], OUTPUT);
    pinMode(bankPin[i], OUTPUT);
    digitalWrite(gpioPin[i], HIGH);
    digitalWrite(bankPin[i], LOW); // The bank pin supplies 3.3V for the optocouplers supply. LOW disables the particular bank
  }
  pinMode(15,OUTPUT);
  digitalWrite(15,HIGH);
  pickBattery(-1);
  pickBattery(0);
  currentCharger = 0;
  lastMinutes = runMinutes;
  lastDay =runDays;
}

// Change the relays, either to the next relay or scan all for voltages
int pickBattery(int num) {
  for(int i = 0; i < RELAYS; i++) {
     digitalWrite(gpioPin[i], HIGH);
  }
  if(num >= 0 && !allCharged) { //
    digitalWrite(gpioPin[num], LOW);
    digitalWrite(bankPin[num], HIGH);
  } else { // scan all the batteries for voltage
    allCharged = 1;
    for(int i = 0; i < RELAYS; i++){
      digitalWrite(gpioPin[i], LOW);
      digitalWrite(bankPin[num], HIGH);
      delay(100);
      batVolts[i] = analogRead(sensorPin) * CALIBRATION;
      if (batVolts[i] > fullyCharged) 
        batteryCharged[i] =1;
      else
        batteryCharged[i] =0;
      if (batVolts[i] > 4.0)
        batteryConnected[i] =1;
      else
        batteryConnected[i] = 0;
      if (batteryConnected[i] && !batteryCharged[i]) {
          allCharged = 0;
          lastDay = runDays;
      } else {
        lastDay = runDays +1;
      }
        
      Serial.print(" ");
      Serial.print(batVolts[i]);
      digitalWrite(gpioPin[i], HIGH);
    }
    Serial.println(" ");
    
  }
}


void loop() {
#if defined(SOFTAP)
  dnsServer.processNextRequest();
#endif
  webServer.handleClient(); 
  if(!allCharged) batVolts[currentCharger] = analogRead(sensorPin) * CALIBRATION; //If no battery is selected the analog on battery1 is erronious
  // See if we have rolled over, delayed enough or current battery has < 1.2V
  // if all batteries are charged, pickBattery will set the lastDay 1 in front so that it waits until then to attempt charging again
  if ((!allCharged && runMinutes >= (lastMinutes + CHARGEMINUTES[(int)batVolts[currentCharger]]))||( runMinutes < lastMinutes)) {
    lastMinutes = runMinutes;
    lastDay = runDays;
    if (currentCharger < RELAYS ) {
      pickBattery(currentCharger);
      currentCharger++;
    } else {
      currentCharger = 0;
      pickBattery(-1);
    }     
    //currentCharger++;
      
  }
  // Read the battery voltage each hour if fully charged
  if ((runHours >lastHour) && allCharged){
    pickBattery(-1);
    lastHour = runHours;
  }
  //Serial.println(responseHTML);

  // Look for reset command from USB
  serialEvent();
  if (stringComplete) {
    Serial.println(inputString);
    if (inputString.substring(0)=="reset") {
       Serial.println("Resetting Wifi Configuration");
       WiFi.disconnect(true);
       mySmartConfig();
       delay(5000);
       ESP.restart();
    }
      //clear the string:
    inputString = "";
    stringComplete = false;
  }
  delay(200);
  
  
}
