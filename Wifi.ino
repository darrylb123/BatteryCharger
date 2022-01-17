// Wifi Functions choose between Station or SoftAP



void wifiStartup(){
  // Build Hostname
  snprintf(sapString, 20, "BatteryCharger-%08X", ESP.getChipId());

  // Attempt to connecty to the AP stored on board, if not, start in SoftAP mode
  WiFi.mode(WIFI_STA);
  delay(2000);
  WiFi.begin();
  delay(5000);
  
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    Serial.print("Running in AP mode AP name: ");
    Serial.println(sapString);
    delay(2000);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(sapString);
    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);
    needDNS = 1;
  } else {
  
    String hostName = "Hostname: ";
    hostName = hostName + sapString;
    Serial.println(hostName);
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    needDNS = 0;
  } 


}

// Configure wifi using ESP Smartconfig app on phone
int mySmartConfig() {
  // Wipe current credentials
  WiFi.disconnect(true); // deletes the wifi credentials
  
  WiFi.mode(WIFI_STA);
  delay(2000);
  WiFi.begin();
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
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}



int wifiLoop(){
  if (needDNS) {
    dnsServer.processNextRequest();
  }
}
