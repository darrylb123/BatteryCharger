#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

#define FORMAT_LITTLEFS_IF_FAILED 
#define RELAYS 7
#define CALIBRATION 0.0157521

// Our configuration structure.
//
// Never use a JsonDocument to store the configuration!
// A JsonDocument is *not* a permanent storage; it's only a temporary storage
// used during the serialization phase. See:
// https://arduinojson.org/v6/faq/why-must-i-create-a-separate-config-object/


const char *confFile = "/config.txt";  


// Loads the configuration from a file
void loadConfiguration(const char *filename, Config &config) {
  // Open file for reading
  File file = LittleFS.open(filename,"r");

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<1024> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonDocument to the Config
  config.calConst = doc["calConst"] | CALIBRATION;                 
  config.mqttBroker =     doc["mqttBroker"] | "";
  config.mqttPort = doc["mqttPort"] | 1883 ;
  config.mqttUser =     doc["mqttUser"] | "";
  config.mqttPasswd =     doc["mqttPasswd"] | "";
  char tmpstr[50];
  sprintf(tmpstr,"%s/data",sapString);
  config.mqttTopic = doc["mqttTopic"] | tmpstr;
  for(int i = 0; i < RELAYS;i++){
    char tmpstr[30];
    sprintf(tmpstr,"Label %d",i+1);
    config.labelTXT[i] = doc["labelTXT"][i] | tmpstr;
  }
  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
  
}

// Saves the configuration to a file
void saveConfiguration(const char *confFile, const Config &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  LittleFS.remove(confFile);

  // Open file for writing
  File file = LittleFS.open(confFile, "w");
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Set the values in the document
  doc["mqttBroker"] = config.mqttBroker;
  doc["mqttPort"] = config.mqttPort;
  doc["mqttUser"] = config.mqttUser;
  doc["mqttPasswd"] = config.mqttPasswd;
  doc["mqttTopic"] = config.mqttTopic;  
  doc["calConst"] = config.calConst;
  for(int i = 0; i < RELAYS;i++){
    doc["labelTXT"][i] = config.labelTXT[i];
  }
  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
}

// Prints the content of a file to the Serial
void printFile(const char *confFile) {
  // Open file for reading
  File file = LittleFS.open(confFile,"r");
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
}

void configSetup() {
  // Initialize serial port
  Serial.begin(115200);
  while (!Serial) continue;
  
  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println(F("LittleFS mount failed"));
    return;
  }

  // Should load default config if run for the first time
  Serial.println(F("Loading configuration..."));
  loadConfiguration(confFile, config);

  // Create configuration file
  Serial.println(F("Saving configuration..."));
  saveConfiguration(confFile, config);

  // Dump config file
 Serial.println(F("Print config file..."));
  printFile(confFile);
}

void rmfiles(){
  if (LittleFS.remove("/config.txt")) {
    Serial.println("/config.txt removed");
  } else {
    Serial.println("/config.txt removal failed");
  }
}
