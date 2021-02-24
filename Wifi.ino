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
