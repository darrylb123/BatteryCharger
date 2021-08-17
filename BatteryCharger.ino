/* Battery Charger Multiplexer using ESP8266 or ESP32*/
/*
Copyright (C) 2021  Darryl Bond
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#define STATION 1
// #define SOFTAP 1
#define VER2 1
#define DEBUG 0
#include <stdio.h>
#include <Ticker.h>

#if defined(VER2)
#define ENERG LOW
#define DEENERG HIGH
#else
#define ENERG HIGH
#define DEENERG LOW
#endif

#if defined(SOFTAP)
#include "DNSServer.h"                  // Patched lib
// Capture DNS requests on port 53
IPAddress         apIP(10, 10, 10, 1);    // Private network for server
DNSServer         dnsServer;              // Create the DNS object

#endif

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#endif


// An array of charging time indexed by the measured voltage. Allows a flat battery to be charged longer and a fully charged battery to be charged for a short time
// Volts                     0, 1,  2,  3,  4,  5,  6,  7,  8,    9,    10,   11,   12, 13, 14, 15, 16
const int CHARGEMINUTES[] = {1, 1,  1,  1,  1,  30, 30, 30, 240,  240,  240,  100,  10, 1,  1,  1,  1};
const float fullyCharged = 11.9;
const byte        DNS_PORT = 53;
String inputString;
int stringComplete;


Ticker minutes;



// Wemos D1R2 only supports 8 relays 
// Wemos R32 supports 16 relays
#if defined(ESP8266)
const int RELAYS = 7;
#if defined(VER2) 
const int gpioPin[] = { 16, 14, 12, 13, 15, 0, 4 };
const int chargeDisable[] = { 5, 5, 5, 5, 5, 5, 5 };
const int bankPin[] = { 3, 3, 3, 3, 3, 3, 3 }; // Relay bank enable pin
const float CALIBRATION = 0.01536643; //15k/1k ohm resistors. Analog in is 0-1V (12/782 measured)
#else
const int gpioPin[] = { 16, 5, 4, 0, 2, 14, 12 };
const int chargeDisable[] = { 13, 13, 13, 13, 13, 13, 13 };
const int bankPin[] = { 15, 15, 15, 15, 15, 15, 15 }; // Relay bank enable pin
const float CALIBRATION = 0.014273256; // 8.2/2.2 ohm resistor divider (3.313 / .694V = 234 of 1024 )
#endif

const int sensorPin = A0;



#elif defined(ESP32)
const int RELAYS = 14; 
const int gpioPin[] =       { 26, 25, 17, 16, 27, 14, 12, 26, 25, 17, 16, 27, 14, 12 };
const int chargeDisable[] = { 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13 };
const int bankPin[] =       {  5,  5,  5,  5,  5,  5,  5, 23, 23, 23, 23, 23, 23, 23 }; // Relay bank enable pin
const float CALIBRATION = 0.004126132; // 8.2/2.2 ohm resistor divider (5V / 1/074V = 1128 of 4096 )
const int sensorPin = 34;
#endif

const int SCAN = 1000; // Delay in ms between each battery when scanning 
const int TESTCYCLE = 15; // Cycle time for testing batteries
char sapString[30]; // SSID and mqtt name unique by reading chip ID
float batVolts[RELAYS];
int batteryCharged[RELAYS];
int batteryConnected[RELAYS];
int allCharged = 0;
int currentCharger = 0 ;
int sensorValue = 0;
int firstBoot = 1;
int lastMinutes, lastHour, lastDay;
int runMinutes = 0;
int runHours = 0;
int runDays = 0;
char *responseHTML;
String labelTXT[RELAYS];

void setup() {
  Serial.begin(115200);
  // Start Minute Ticker
  // Ticker is where all the action happens
  minutes.attach(60, eachMinute);
  
  wifiStartup();
  initialiseWebUI();

  // Set direction of IO
  for (int i = 0; i < RELAYS; i++) {
    pinMode(gpioPin[i], OUTPUT);
    pinMode(bankPin[i], OUTPUT);
    pinMode(chargeDisable[i], OUTPUT);
    digitalWrite(gpioPin[i], ENERG);
    digitalWrite(bankPin[i], DEENERG); // The bank pin supplies 3.3V for the optocouplers supply. DEENERG disables the particular bank
  }
  
  scanAll(); // Scan all Batteries on boot
  batteriesCharged(); // See if they are allCharged
  
  currentCharger = 0;
  if (!allCharged) 
    pickBattery(currentCharger); //Start with the first battery
  lastMinutes = runMinutes;
  lastDay = runDays;
  firstBoot = 1;
  
  delay(1000);
}


// Function Runs from the minute ticker
// Does all the work with deciding when to change relays
void eachMinute (){
  // Routine runs every minute
  logbat();
  runMinutes++;
  if (runMinutes < lastMinutes){
    lastMinutes = runMinutes;
    lastDay = runDays;
  }
  if ( ! (runMinutes % TESTCYCLE) ) {
    runHours++;
  }
  if ( ! (runMinutes % 1440) ) {
    runDays++;
  }
  // Check if the charge time on current has expired and increment battery number
  // See if we have rolled over, delayed enough or current battery has < 1.2V
  // if all batteries are charged, pickBattery will set the lastDay 1 in front so that it waits until then to attempt charging again
  if (runMinutes >= (lastMinutes + CHARGEMINUTES[(int)batVolts[currentCharger]]) ) {
    lastMinutes = runMinutes;
    lastDay = runDays;
    currentCharger++;
  }
  
  if (allCharged)
    currentCharger = 0;

  if (currentCharger >= RELAYS ) { // At the end of a cycle check all the batteries to see if any need charging again
    currentCharger = 0;
    scanAll(); 
  }
  // Check if all batteries are charged. pickBattery wont charge any if all charged
  pickBattery(currentCharger);
}

int scanAll(){
  int j;
  // scan all the batteries for voltage
  // Disable the charger to read actual battery volts
  Serial.println("Scanning");
  // Initialise all pins to off
  
  // Set all Charged to off to allow reading voltages
  allCharged = 0;
  currentCharger = 0;
  for (int i = 0; i < RELAYS; i++) {
    for (int i = 0; i < RELAYS; i++) {
      digitalWrite(gpioPin[i], ENERG);
      digitalWrite(chargeDisable[i], ENERG);
      digitalWrite(bankPin[i], DEENERG);
    }
    j = millis();
    while(millis() < (j+SCAN)){
      webLoop();
    }
    digitalWrite(bankPin[i], ENERG);
    delay(100); // Allow time to power the module
    digitalWrite(gpioPin[i], DEENERG);
    digitalWrite(chargeDisable[i], DEENERG);
    delay(100);
    j = millis();
    while(millis() < (j+500)){
      batteryState(i);
      webLoop();
    }
    
    Serial.print(" ");
    Serial.print(batVolts[i]);
    Serial.print(" ");
    Serial.print(batteryCharged[i]);
    Serial.print(batteryConnected[i]);
  }
  Serial.println(" ");
  for (int i = 0; i < RELAYS; i++) {
    digitalWrite(gpioPin[i], ENERG);
    digitalWrite(chargeDisable[i], ENERG);
    digitalWrite(bankPin[i], DEENERG);
  }
  batteriesCharged();
}

// Check if all the batteries are charged
int batteriesCharged(){
  allCharged = 1;
  for (int i = 0; i < RELAYS; i++) {
    if (batteryConnected[i] && !batteryCharged[i]) {
      allCharged = 0;
    }
  }
  return(allCharged);
}
// Change the relays, either to the next relay or scan all for voltages
int pickBattery(int num) {
  for (int i = 0; i < RELAYS; i++) {
    digitalWrite(gpioPin[i], ENERG);
    digitalWrite(bankPin[i], DEENERG);
  }
  if (num < RELAYS && !batteriesCharged()) { // if the number is the same as NUMBER OF RELAYS then check all
    digitalWrite(gpioPin[num], DEENERG);
    digitalWrite(bankPin[num], ENERG);
  } 
}

int batteryState(int batnum) { 
  if (batnum >= RELAYS) {
    Serial.println("Error: Past End of RELAYS Array");
    return(0);
  }
  // if(allCharged) return(0);
  
  int rel = digitalRead(gpioPin[batnum]);
  int bank = digitalRead(bankPin[batnum]);
  int enabled = digitalRead(chargeDisable[batnum]);

  if ( enabled == ENERG ) {
    Serial.println("Charger is enabled, no point reading voltage");
    return(0); 
  }
  if ( rel == ENERG || bank == DEENERG ) {
    char err[50];
    sprintf(err,"Error Relay %d not zero %d or bank one %d",batnum + 1, rel, bank);
    Serial.println(err);
    return(0);
  }
  
  batVolts[batnum] = analogRead(sensorPin) * CALIBRATION;
  if (batVolts[batnum] > fullyCharged)
      batteryCharged[batnum] = 1;
  else
      batteryCharged[batnum] = 0;
  if (batVolts[batnum] > 4.0 && batVolts[batnum] < 15.2) { // Some chargers have 16V or so open circuit
      batteryConnected[batnum] = 1;
  } else {
      batteryConnected[batnum] = 0;
      batteryCharged[batnum] = 0;
  }
  return(batteryCharged[batnum]);
}


void loop() {
  wifiLoop();
  webLoop();
  
  // Read the battery voltage each hour if fully charged
  if ((runHours > lastHour) && batteriesCharged()) {
    scanAll();
    lastHour = runHours;
  }
  // delay(50); // Waste a little time 
}

void logbat () {
  int t = analogRead(sensorPin);
  String bat = "Selected battery ";
  String theTime = "Run Time: ";
  theTime = theTime + " " + runMinutes + " minutes " + runHours + " hours " + runDays + " days ";
  int num = currentCharger ;
  bat = theTime + " " + bat + num + " " + t + " " + String(t * CALIBRATION, 2);
  if (allCharged)
    bat = bat + " All Charged";
  Serial.println(bat);
}
