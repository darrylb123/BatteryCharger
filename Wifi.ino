// Wifi Functions choose between Station or SoftAP
#include <WiFiManager.h>

char ssid[] = "";
char pw[] = "";

void wifiStartup(){
  // Build Hostname
  snprintf(sapString, 20, "BatteryCharger-%08X", ESP.getChipId());

  // Attempt to connecty to the AP stored on board, if not, start in SoftAP mode
  WiFi.mode(WIFI_STA);
  delay(2000);
  WiFi.hostname(sapString);
  if(strlen(ssid)) {
    Serial.print("Using supplied ssid/pw");
    WiFi.begin(ssid,pw);
  } else {
    Serial.print("Using stored ssid/pw");
    WiFi.begin();
  }
  
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
void mySmartConfig() {

  WiFiServer server(80);
  // Wipe current credentials
  WiFi.disconnect(true); // deletes the wifi credentials
  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  
  // Uncomment and run it once, if you want to erase all the stored information
  wifiManager.resetSettings();
  
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect(sapString);
  // or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();
  
  // if you get here you have connected to the WiFi
  Serial.println("Connected.");
  ESP.restart();
  server.begin();

}



void wifiLoop(){
  if (needDNS) {
    dnsServer.processNextRequest();
  }
}
