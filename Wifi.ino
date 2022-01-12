

// Wifi Functions choose between Station or SoftAP

#if defined(STATION)
#if defined(ESP32)
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin();
}
#endif
#endif

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
  buildHostname();
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
  delay(2000);
  WiFi.begin();
  #if defined(ESP32)
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  #endif
  
  delay(5000);
  if (WiFi.status() != WL_CONNECTED) {
    mySmartConfig();
  }
  
  String hostName = "Hostname: ";
  hostName = hostName + sapString;
  Serial.println(hostName);
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  // delay(500);  

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
