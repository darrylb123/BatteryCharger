/*
   Captive Portal by: M. Ray Burnette 20150831
   See Notes tab for original code references and compile requirements
   Sketch uses 300,640 bytes (69%) of program storage space. Maximum is 434,160 bytes.
   Global variables use 50,732 bytes (61%) of dynamic memory, leaving 31,336 bytes for local variables. Maximum is 81,920 bytes.
*/

#include <ESP8266WiFi.h>
#include "./DNSServer.h"                  // Patched lib
#include <ESP8266WebServer.h>

const int RELAYS = 8;
const int CHARGEMINUTES = 30;
const int CHARGETIME = CHARGEMINUTES * 60 * 1000; // 30 Minutes
const byte        DNS_PORT = 53; 
const int sensorPin = A0;
const float CALIBRATION = 0.014273256; // 8.2/2.2 ohm resistor divider (3.313 / .694V = 234 of 1024 )
// Capture DNS requests on port 53
IPAddress         apIP(10, 10, 10, 1);    // Private network for server
DNSServer         dnsServer;              // Create the DNS object
ESP8266WebServer  webServer(80);          // HTTP server

int gpioPin[] = { 16,5,4,14,12,13,0,2 }; // NodeMCU pin definitions


int batVolts[RELAYS];
int currentCharger = -1 ;                   
int sensorValue = 0;
int firstBoot = 1;
int32_t last_global_millis = 0;
int32_t new_global_millis = 0;

void setup() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Battery Charger");
  Serial.begin(115200);
  
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);
  // replay to all requests with same HTML
  webServer.onNotFound([]() {

    String headerHTML =   "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"10\">\
                      <title>CaptivePortal</title></head><body>\
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

    String bodyHTML =  "<H1> Battery Charger - ";
    bodyHTML = bodyHTML + CHARGEMINUTES + " minutes/battery </H1>";

    String footerHTML =   "</body></html>";
    
    int minutes = ((CHARGETIME -(new_global_millis - last_global_millis )) / 60000) + 1; // Calculate charging time on this battery
    String batString = "<TABLE><TR><TH>Battery</TH><TH>Volts</TH><TH>Minutes</TH>\n";
   
    int j;
    for (int i = 0; i <RELAYS; i++){
      j=i+1;
      if ( i == currentCharger )
        batString = batString + "<TR><TD style=\"background-color:Tomato;\">" + j + "</TD><TD>"+ String(batVolts[i] * CALIBRATION, 2) + " (" +batVolts[i] + ")<TD>" + minutes + "</TD></TR>\n";
      else batString = batString + "<TR><TD>" + j + "</TD><TD>"+ String(batVolts[i] * CALIBRATION, 2)+ " (" +batVolts[i] + ")</TD><TD></TD></TR>\n";
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
  last_global_millis = millis();
}

int pickbattery(int num) {
  for(int i = 0; i < RELAYS; i++) {
     digitalWrite(gpioPin[i], HIGH);
  }
  digitalWrite(gpioPin[num], LOW);
  String bat = "Selected battery ";
  bat = bat + num;
  Serial.println(bat);
}


void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient(); 
  batVolts[currentCharger] = analogRead(sensorPin);
  new_global_millis = millis();
  // See if we have rolled over, delayed enough or current battery has < 1.2V
  if (( new_global_millis > (last_global_millis + CHARGETIME))
  ||( new_global_millis < last_global_millis) 
  || (batVolts[currentCharger] < 100 )
  || firstBoot ) {
    last_global_millis = new_global_millis;
    if (currentCharger >= RELAYS-1 ) {
      currentCharger = -1;
      firstBoot = 0;
    }
      
      
      pickbattery(++currentCharger);
  }
  //Serial.println(responseHTML);
  
  delay(500);
  
}
