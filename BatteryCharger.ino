/* Battery Charger Multiplexer using ESP8266 or ESP32*/
#define STATION 1
// #define SOFTAP 1
#define DEBUG 0
#include <stdio.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>
#include <FS.h>

#include <LittleFS.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <Ticker.h>
#include <LITTLEFS.h>
#endif

#if defined(SOFTAP)
#include "DNSServer.h"                  // Patched lib
// Capture DNS requests on port 53
IPAddress         apIP(10, 10, 10, 1);    // Private network for server
DNSServer         dnsServer;              // Create the DNS object

#endif

// An array of charging time indexed by the measured voltage. Allows a flat battery to be charged longer and a fully charged battery to be charged for a short time
// Volts                     0, 1,  2,  3,  4,  5,  6,  7,  8,    9,    10,   11,   12, 13, 14, 15, 16
const int CHARGEMINUTES[] = {1, 1,  1,  1,  1,  30, 30, 30, 240,  240,  240,  100,  30, 1,  1,  1,  1};
const float fullyCharged = 11.9;
const byte        DNS_PORT = 53;
String inputString;
int stringComplete;

Ticker logger;
Ticker minutes;


#if defined(ESP8266)
ESP8266WebServer  webServer(80);          // HTTP server
#define MYFS LittleFS
#define FORMAT_LITTLEFS_IF_FAILED 

#elif defined(ESP32)
WebServer  webServer(80);
#define MYFS LITTLEFS
#define FORMAT_LITTLEFS_IF_FAILED true
#endif

// Wemos D1R2 only supports 8 relays 
// Wemos R32 supports 16 relays
#if defined(ESP8266)
const int RELAYS = 8;
const int gpioPin[] = { 16, 5, 4, 0, 2, 14, 12, 13 };
const int bankPin[] = { 15, 15, 15, 15, 15, 15, 15, 15 }; // Relay bank enable pin
const int sensorPin = A0;
const float CALIBRATION = 0.014273256; // 8.2/2.2 ohm resistor divider (3.313 / .694V = 234 of 1024 )


#elif defined(ESP32)
const int RELAYS = 16; 
const int gpioPin[] = { 26, 25, 17, 16, 27, 14, 12, 13, 26, 25, 17, 16, 27, 14, 12, 13 };
const int bankPin[] = {5, 5, 5, 5, 5, 5, 5, 5, 23, 23, 23, 23, 23, 23, 23, 23 }; // Relay bank enable pin
const float CALIBRATION = 0.004042956; // 8.2/2.2 ohm resistor divider (5V / 1/074V = 1128 of 4096 )
const int sensorPin = 34;
#endif

const int SCAN = 50; // Delay in ms between each battery when scanning 
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

void logbat () {
  int t = analogRead(sensorPin);
  String bat = "Selected battery ";
  runMinutes++;
  if ( ! (runMinutes % 60) ) {
    runHours++;
  }
  if ( ! (runMinutes % 1440) ) {
    runDays++;
  }
  String theTime = "Run Time: ";
  theTime = theTime + " " + runMinutes + " minutes " + runHours + " hours " + runDays + " days ";
  int num = currentCharger ;
  bat = theTime + " " + bat + num + " " + t + " " + String(t * CALIBRATION, 2);
  if (allCharged)
    bat = bat + " All Charged";
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
// Label form page
void formPage () {
  char tempstr[512];

  strcpy(responseHTML, "<!DOCTYPE html><html><head>\
                      <title>Battery Charger</title></head><body>\
                      <style>\
                      table {\
  border-collapse: collapse;\
  width: 100%;\
  font-size: 30px;\
}\
\
table, th, td {\
  border: 1px solid black;\
}\
</style>\
<H1> Battery Charger Labels</H1>\
<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/postform/\">");

  sprintf(tempstr, "<H3>Labels for each battery</H3>\n");
  strcat(responseHTML, tempstr);

  strcat(responseHTML, "<TABLE><TR><TH>Battery</TH><TH>Label</TH>\n");

  for (int i = 0; i < RELAYS; i++) {
    char tmplabel[50];
    labelTXT[i].toCharArray(tmplabel,sizeof(tmplabel));
    sprintf(tempstr, "<TR><TD> %d </TD><TD><input type=\"text\" name=\"%d\" value=\"%s\"> </TD><TR>\n",
              i + 1,i + 1,tmplabel);
    strcat(responseHTML, tempstr);
  }
  strcat(responseHTML, "</TABLE>");
  strcat(responseHTML, "<input type=\"submit\" value=\"Submit\">\
    </form>\
    <A href=\"/\">Return to Battery Display</A>\
    </body></html>\n");
  if (DEBUG)
    Serial.print(responseHTML);
  delay(100);// Serial.print(responseHTML);
  webServer.send(200, "text/html", responseHTML);

}

// Web Server page handler
void handlePage () {
  char tempstr[1024];

  strcpy(responseHTML, "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"20\">\
                      <title>Battery Charger</title></head><body>\
                      <style>\
                      table {\
  border-collapse: collapse;\
  width: 100%;\
  font-size: 30px;\
}\
\
table, th, td {\
  border: 1px solid black;\
}\
</style>\
<H1> Battery Charger </H1>\
");

  sprintf(tempstr, "<H3>Time since boot %d minutes %d hours %d days</H3>\n", runMinutes, runHours, runDays);
  strcat(responseHTML, tempstr);

  strcat(responseHTML, "<TABLE><TR><TH>Battery</TH><TH>Volts</TH><TH>Minutes</TH><TH>Flags</TH>\n");

  int minutes = CHARGEMINUTES[(int)batVolts[currentCharger]] - ( runMinutes - lastMinutes ) ; // Calculate charging time on this battery
  for (int i = 0; i < RELAYS; i++) {
    char tmplabel[50];
    labelTXT[i].toCharArray(tmplabel,sizeof(tmplabel));
    if ( i == currentCharger && !allCharged) {
      sprintf(tempstr, "<TR><TD style=\"background-color:Tomato;\"> %d (%s)</TD><TD> %5.2f </TD> <TD> %d ( %d ) </TD><TD> %d %d </TD><TR>\n",
              i + 1, tmplabel  , batVolts[i], CHARGEMINUTES[(int)batVolts[i]], minutes, batteryCharged[i], batteryConnected[i]);
    } else {
      
      sprintf(tempstr, "<TR><TD> %d (%s) </TD><TD> %5.2f </TD> <TD> %d  </TD><TD> %d %d </TD><TR>\n",
              i + 1, tmplabel  ,batVolts[i], CHARGEMINUTES[(int)batVolts[i]], batteryCharged[i], batteryConnected[i]);
    }
    strcat(responseHTML, tempstr);
  }
  strcat(responseHTML, "</TABLE>");
  if (allCharged) {
    sprintf(tempstr, "<H3>All Charged, delaying next charge cycle until needed, scan in %d minutes<h3>\n", (60 - (runMinutes % 60)));
    strcat(responseHTML, tempstr);
  } 
  strcat(responseHTML, "<A href=\"/editlabels\">Edit Battery Labels</A> </body></html>\n");
  if (DEBUG)
    Serial.print(responseHTML);
  delay(100);// Serial.print(responseHTML);
  webServer.send(200, "text/html", responseHTML);

}

// Function to extract the new label text and update the labels array. The labels are then written to the labels.txt file
void handleForm() {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "text/plain", "Method Not Allowed");
  } else {
    String message = "POST form was:\n";
    for (uint8_t i = 0; i < webServer.args(); i++) {
      String name = webServer.argName(i);
      long whichlabel = name.toInt() -1;
      Serial.println(name + "" + whichlabel);
      if (whichlabel > -1 && whichlabel < RELAYS)
        labelTXT[whichlabel] = webServer.arg(i);
      message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
    }
    webServer.send(200, "text/plain", message);
    Serial.print(message);
    writeFile();
  }
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
  logger.attach(60, logbat);
  responseHTML = (char *)malloc(15000);
#if defined(ESP32)
  uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
  uint16_t chip = (uint16_t)(chipid >> 32);
  snprintf(sapString, 20, "BatteryCharger-%04X", chip); 
#elif defined(ESP8266)
  snprintf(sapString, 20, "BatteryCharger-%08X", ESP.getChipId());
#endif
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
  #if defined(ESP32)
  WiFi.setHostname(sapString); // this sets a unique hostname for DHCP
  #elif defined(ESP8266)
  WiFi.hostname(sapString); // this sets a unique hostname for DHCP
  #endif
  WiFi.begin();
  delay(5000);
  if (WiFi.status() != WL_CONNECTED) {
    mySmartConfig();
  }

  Serial.println("WiFi Connected.");
  String hostName = sapString;
  hostName = hostName + " ";
  Serial.print(hostName);
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  delay(500);
#endif
  webServer.on("/", handlePage);
  webServer.on("/postform/", handleForm);
  webServer.on("/rmfiles", rmfiles);
  webServer.on("/labels", labels);
  webServer.on("/editlabels", formPage);
  webServer.onNotFound(handlePage);
  webServer.begin();

  for (int i = 0; i < RELAYS; i++) {
    pinMode(gpioPin[i], OUTPUT);
    pinMode(bankPin[i], OUTPUT);
    digitalWrite(gpioPin[i], HIGH);
    digitalWrite(bankPin[i], LOW); // The bank pin supplies 3.3V for the optocouplers supply. LOW disables the particular bank
  }
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  pickBattery(RELAYS);
  currentCharger = 0;
  lastMinutes = runMinutes;
  lastDay = runDays;
  firstBoot = 1;
  // Initialise the label arrays
  for ( int i = 0; i < RELAYS; i++ ) {
    int j = i+1;
    labelTXT[i] = "Label " ;
    labelTXT[i] = labelTXT[i] + j;
  }
  // Create the filesystem 
  // LittleFS.format();
  Serial.println("Mount LittleFS");
 // MYFS.format();
  if (!MYFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("LittleFS mount failed");
    return;
  }


  // Read the labels file and  write to label array
  // else initialise the labels file 
  if (MYFS.exists("/labels.txt")) {
    int lcount = 0;
    char buffer[50];
    File labelf = MYFS.open("/labels.txt", "r");
    while (labelf.available()) {
      int l = labelf.readBytesUntil('\n', buffer, sizeof(buffer));
      buffer[l] = 0;
      labelTXT[lcount++] = buffer;
      Serial.println(buffer);
      if(lcount >= RELAYS) break;
    }
  } else {
    writeFile();   
  }
  delay(1000);
}

void writeFile() {
  char path[] = "/labels.txt";
  Serial.printf("Writing file: %s\n",path);

  File file = MYFS.open(path, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  for ( int i = 0; i < RELAYS; i++ ) {
    if (!file.println( labelTXT[i] ))
      Serial.println("Write failed");
  }
  file.close();
}
void rmfiles(){
  if (MYFS.remove("/labels.txt")) {
    Serial.println("/labels.txt removed");
  } else {
    Serial.println("/labels.txt removal failed");
  }
}

void labels() {
  Serial.println("Reading file: /labels.txt");

  File file = MYFS.open("/labels.txt", "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}


// Change the relays, either to the next relay or scan all for voltages
int pickBattery(int num) {
  for (int i = 0; i < RELAYS; i++) {
    digitalWrite(gpioPin[i], HIGH);
    digitalWrite(bankPin[i], LOW);
  }
  if (num < RELAYS && !allCharged) { // if the number is the same as NUMBER OF RELAYS then check all
    digitalWrite(gpioPin[num], LOW);
    digitalWrite(bankPin[num], HIGH);
  } else { // scan all the batteries for voltage
    allCharged = 1;
    Serial.println("Scanning");
    for (int i = 0; i < RELAYS; i++) {
      digitalWrite(bankPin[i], HIGH);
      digitalWrite(gpioPin[i], LOW);
      delay(SCAN);
      batVolts[i] = analogRead(sensorPin) * CALIBRATION;
      if (batVolts[i] > fullyCharged)
        batteryCharged[i] = 1;
      else
        batteryCharged[i] = 0;
      if (batVolts[i] > 4.0)
        batteryConnected[i] = 1;
      else
        batteryConnected[i] = 0;
      if (batteryConnected[i] && !batteryCharged[i]) {
        allCharged = 0;
        lastDay = runDays;
      } else {
        lastDay = runDays + 1;
      }

      Serial.print(" ");
      Serial.print(batVolts[i]);
      digitalWrite(gpioPin[i], HIGH);
      digitalWrite(bankPin[i], LOW);
    }
    Serial.println(" ");

  }
}


void loop() {
#if defined(SOFTAP)
  dnsServer.processNextRequest();
#endif
  webServer.handleClient();
  if (!allCharged && currentCharger < RELAYS) batVolts[currentCharger] = analogRead(sensorPin) * CALIBRATION; //If no battery is selected the analog on battery1 is erronious
  // See if we have rolled over, delayed enough or current battery has < 1.2V
  // if all batteries are charged, pickBattery will set the lastDay 1 in front so that it waits until then to attempt charging again
  if ((!allCharged && runMinutes >= (lastMinutes + CHARGEMINUTES[(int)batVolts[currentCharger]])) || ( runMinutes < lastMinutes)) {
    lastMinutes = runMinutes;
    lastDay = runDays;
    if (firstBoot) {
      firstBoot = 0;
    } else {
      currentCharger++;
    }

    if (currentCharger >= RELAYS ) {
      currentCharger = 0; 
      pickBattery(RELAYS);
    }
    pickBattery(currentCharger);

  }
  // Read the battery voltage each hour if fully charged
  if ((runHours > lastHour) && allCharged) {
    pickBattery(RELAYS);
    lastHour = runHours;
  }
  //Serial.println(responseHTML);

  // Look for reset command from USB
  serialEvent();
  if (stringComplete) {
    Serial.println(inputString);
    if (inputString.substring(0) == "reset") {
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
