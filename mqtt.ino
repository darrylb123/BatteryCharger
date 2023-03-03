
#if defined(NEEDMQTT)
#include <PubSubClient.h>


WiFiClient espClient;
PubSubClient client(espClient);

void brokerConnect() {
  if(config.mqttBroker.length() < 1){
    return;
  }
  
  Serial.println("Connecting to MQTT Broker");
  client.setServer(config.mqttBroker.c_str(), config.mqttPort);
  
  // client.setCallback(callback);
  for(int i =0; i < 3;i++) {
    if ( !client.connected()){
      String client_id = "esp8266-client-";
      client_id += String(WiFi.macAddress());
      Serial.printf("The client %s connects to the mqtt broker %s", client_id.c_str(),config.mqttBroker.c_str());
      // If there is a user account
      if(config.mqttUser.length() > 1){
        Serial.println(" with user/password");
        if (client.connect(client_id.c_str(),config.mqttUser.c_str(),config.mqttPasswd.c_str())) {
        } else {
          Serial.print("failed with state ");
          Serial.print(client.state());
          delay(2000);
        }
      } else {
        Serial.println(" without user/password");
        if (client.connect(client_id.c_str())) {
        } else {
          Serial.print("failed with state ");
          Serial.print(client.state());
          delay(2000);
        }
      }
    }
  }
}
void publishData(){
  brokerConnect();
  if (client.connected()){
    char theData[2000];
    char tmpstr[100];
    sprintf(theData,"{ \"Volts\":{\"C1\":%4.2f,\"C2\":%4.2f,\"C3\":%4.2f,\"C4\":%4.2f,\"C5\":%4.2f,\"C6\":%4.2f,\"C7\":%4.2f}, \"Label\":{",batVolts[0],batVolts[1],batVolts[2],batVolts[3],batVolts[4],batVolts[5],batVolts[6]);
    for(int i=0; i< RELAYS;i++){
      sprintf(tmpstr,"\"C%d\":\"%s\",",i+1,config.labelTXT[i].c_str());
      strcat(theData,tmpstr);
    }
    // Replace the trailing, with }
    theData[strlen(theData)-1] = '}';
    strcat(theData,"}");
    Serial.print(config.mqttTopic);
    Serial.print(" = ");
    Serial.println(theData);
    client.publish(config.mqttTopic.c_str(),theData);
  }
  
}

#endif
