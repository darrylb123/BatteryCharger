
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
  // brokerConnect();
  if (client.connected()){
    char theData[2000];
    char tmpstr[100];
    /* 
    int connectedCount = 0;
    for ( int i=0; i < RELAYS;i++){
      if (batteryConnected[i])
        connectedCount++;
    }
    sprintf(theData,"{\"Connected\":%d,",connectedCount);
    for(int i=0; i< RELAYS;i++){
      sprintf(tmpstr,"\"%d. %s\":%4.2f,",i+1,config.labelTXT[i].c_str(),batVolts[i]);
      strcat(theData,tmpstr);
    }
    // Replace the trailing, with }
    theData[strlen(theData)-1] = '}';
    // strcat(theData,"}");
    Serial.print(config.mqttTopic);
    Serial.print(" = ");
    Serial.println(theData);
    client.publish(config.mqttTopic.c_str(),theData);
    */
  }
  
}

#endif
