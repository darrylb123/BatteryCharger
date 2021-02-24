#if defined(SOFTAP)
#include "DNSServer.h"                  // Patched lib
// Capture DNS requests on port 53
IPAddress         apIP(10, 10, 10, 1);    // Private network for server
DNSServer         dnsServer;              // Create the DNS object

#endif
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#endif

// Wifi Functions choose between Statoin or SoftAP


// build a unique hostname and SAP string
void buildHostname(){
  #if defined(ESP32)
  uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
  uint16_t chip = (uint16_t)(chipid >> 32);
  snprintf(sapString, 20, "BatteryCharger-%04X", chip); 
#elif defined(ESP8266)
  snprintf(sapString, 20, "BatteryCharger-%08X", ESP.getChipId());
#endif
}

void wifiStartup(){
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


int wifiLoop(){
#if defined(SOFTAP)
  dnsServer.processNextRequest();
#endif
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
