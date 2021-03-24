
#if defined(ESP8266)
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <LittleFS.h>
#elif defined(ESP32)
#include <WebServer.h>
#include <LITTLEFS.h>
#include <Update.h>
#endif
#if defined(ESP8266)
ESP8266WebServer  webServer(80);          // HTTP server
#define MYFS LittleFS
#define FORMAT_LITTLEFS_IF_FAILED 

#elif defined(ESP32)
WebServer  webServer(80);
#define MYFS LITTLEFS
#define FORMAT_LITTLEFS_IF_FAILED true

#endif

const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";
 
// Various functions to build the web pages
void initialiseWebUI(){
  // Allocate web page memory
  responseHTML = (char *)malloc(15000);
  webServer.on("/", handlePage);
  webServer.on("/postform/", handleForm);
  webServer.on("/rmfiles", rmfiles);
  webServer.on("/labels", labels);
  webServer.on("/editlabels", formPage);
  webServer.on("/serverIndex", HTTP_GET, []() {
    webServer.sendHeader("Connection", "close");
    webServer.send(200, "text/html", serverIndex);
  });
  #if defined(ESP8266)
  webServer.on("/update", HTTP_POST, []() {
      webServer.sendHeader("Connection", "close");
      webServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    }, []() {
      HTTPUpload& upload = webServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
    });
  #elif defined(ESP32)
  webServer.on("/update", HTTP_POST, []() {
    webServer.sendHeader("Connection", "close");
    webServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = webServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  #endif
  webServer.onNotFound(handlePage);
  webServer.begin();
  
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

  sprintf(tempstr, "<H3>Time since boot %d minutes %d hours %d days</H3>\n", runMinutes, runHours/(60/TESTCYCLE), runDays);
  strcat(responseHTML, tempstr);

  strcat(responseHTML, "<TABLE><TR><TH>Battery</TH><TH>Volts</TH><TH>Minutes</TH><TH>Charged</TH><TH>Connected</TH>\n");

  int minutes = CHARGEMINUTES[(int)batVolts[currentCharger]] - ( runMinutes - lastMinutes ) ; // Calculate charging time on this battery
  for (int i = 0; i < RELAYS; i++) {
    char tmplabel[50];
    labelTXT[i].toCharArray(tmplabel,sizeof(tmplabel));
    if ( i == currentCharger && !allCharged) {
      sprintf(tempstr, "<TR><TD style=\"background-color:Tomato;\"> %d (%s)</TD><TD> %5.2f </TD> <TD> %d ( %d ) </TD><TD> %d </TD><TD> %d </TD><TR>\n",
              i + 1, tmplabel  , batVolts[i], CHARGEMINUTES[(int)batVolts[i]], minutes, batteryCharged[i], batteryConnected[i]);
    } else {
      
      sprintf(tempstr, "<TR><TD> %d (%s) </TD><TD> %5.2f </TD> <TD> %d  </TD><TD> %d </TD><TD> %d </TD><TR>\n",
              i + 1, tmplabel  ,batVolts[i], CHARGEMINUTES[(int)batVolts[i]], batteryCharged[i], batteryConnected[i]);
    }
    strcat(responseHTML, tempstr);
  }
  strcat(responseHTML, "</TABLE>");
  if (allCharged) {
    sprintf(tempstr, "<H3>All Charged, delaying next charge cycle until needed, scan in %d minutes<h3>\n", (TESTCYCLE - (runMinutes % TESTCYCLE)));
    strcat(responseHTML, tempstr);
  } 
  strcat(responseHTML, "<A href=\"/editlabels\">Edit Battery Labels</A> <BR><A href=\"/serverIndex\">Update Firmware</A> </body></html>\n");
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
    Serial.print(message);
    
    writeFile();
    formPage();
  }
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

int webLoop(){
    webServer.handleClient();
}
