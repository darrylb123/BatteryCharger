

#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <LittleFS.h>

#define MYFS LittleFS
#define FORMAT_LITTLEFS_IF_FAILED 


ESP8266WebServer  webServer(80);          // HTTP server



const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'> (or Reboot)"
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
  responseHTML = (char *)malloc(20000);
  webServer.on("/", handlePage);
  webServer.on("/postform/", handleForm);
  webServer.on("/rmfiles", rmfiles);
  webServer.on("/editlabels", formPage);
  webServer.on("/forcecharge", forceChargeCycle);
  webServer.on("/forcescan", forceScanCycle);
  webServer.on("/connectAP", connectAP);
  webServer.on("/serverIndex", HTTP_GET, []() {
  webServer.sendHeader("Connection", "close");
  webServer.send(200, "text/html", serverIndex);

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

  });
  webServer.onNotFound(handlePage);
  webServer.begin();

}
// Label form page
void formPage () {
  char tempstr[1024];

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
<H1> Battery Charger Configuration</H1>\
<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/postform/\">");


  strcat(responseHTML, "<TABLE><TR><TH>Configuration</TH><TH>Setting</TH>\n");

  for (int i = 0; i < RELAYS; i++) {
    sprintf(tempstr, "<TR><TD>Battery %d Label </TD><TD><input type=\"text\" name=\"%d\" value=\"%s\"> </TD><TR>\n",
              i + 1,i + 1,config.labelTXT[i].c_str());
    strcat(responseHTML, tempstr);
  }


  sprintf(tempstr, "<TR><TD>Calibration Constant(%5f):</TD><TD> Enter Displayed Voltage: <input type=\"text\" name=\"Displayed\" value=\"\"> Enter Expected Voltage: <input type=\"text\" name=\"Expected\" value=\"\"></TD><TR>\n",config.calConst);
  strcat(responseHTML, tempstr);

#if defined(NEEDMQTT)
  sprintf(tempstr, "<TR><TD>MQTT Broker Hostname or IP Address (eg public.mqtthq.com):</TD><TD> <input type=\"text\" name=\"mqttBroker\" value=\"%s\"></TD><TR>\n\n",config.mqttBroker.c_str());
  strcat(responseHTML, tempstr);
  sprintf(tempstr, "<TR><TD>MQTT Broker Port : </TD><TD><input type=\"text\" name=\"mqttPort\" value=\"%d\"></TD><TR>\n\n",config.mqttPort);
  strcat(responseHTML, tempstr);
  sprintf(tempstr, "<TR><TD>MQTT Broker User :</TD><TD> <input type=\"text\" name=\"mqttUser\" value=\"%s\"></TD><TR>\n\n",config.mqttUser.c_str());
  strcat(responseHTML, tempstr);
  sprintf(tempstr, "<TR><TD>MQTT Broker Password :</TD><TD> <input type=\"text\" name=\"mqttPasswd\" value=\"%s\"></TD><TR>\n\n",config.mqttPasswd.c_str());
  strcat(responseHTML, tempstr);
  sprintf(tempstr, "<TR><TD>MQTT Topic :</TD><TD> <input type=\"text\" name=\"mqttTopic\" value=\"%s\"></TD><TR>\n\n",config.mqttTopic.c_str());
  strcat(responseHTML, tempstr);

#endif
  strcat(responseHTML, "</TABLE>");
  strcat(responseHTML, "<input type=\"submit\" value=\"Submit\"></form>");
  strcat(responseHTML, "The Calibration Constant is the factor the Analog to Digital converter value is multiplied with to produce the displayed battery voltage.\
  This can be wrong due to variations in the resistors used on each board.\
  To correct the calibration, note the displayed battery volts on a battery and the measured voltage on the same \
  battery using a voltmeter and enter into the fields above.<br>\n");
  strcat(responseHTML, "<A href=\"/\">Return to Battery Display</A> </body></html>\n");
  if (DEBUG)
    Serial.print(responseHTML);
  delay(100);// Serial.print(responseHTML);
  webServer.send(200, "text/html", responseHTML);

}

// Web Server page handler
void handlePage () {
  char tempstr[1024];

  sprintf(responseHTML, "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"20\">\
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
<H1> %s </H1>\
",sapString);

  sprintf(tempstr, "<H3>Time since boot %d minutes %d hours %d days</H3>\n", runMinutes, runHours/(60/TESTCYCLE), runDays);
  strcat(responseHTML, tempstr);

  strcat(responseHTML, "<TABLE><TR><TH>Battery</TH><TH>Volts</TH><TH>Minutes</TH><TH>Charged</TH><TH>Connected</TH>\n");

  int minutes = CHARGEMINUTES[(int)batVolts[currentCharger]] - ( runMinutes - lastMinutes ) ; // Calculate charging time on this battery
  for (int i = 0; i < RELAYS; i++) {
    if ( i == currentCharger && !allCharged) {
      sprintf(tempstr, "<TR><TD style=\"background-color:Tomato;\"> %d (%s)</TD><TD> %5.2f </TD> <TD> %d ( %d ) </TD><TD> %d </TD><TD> %d </TD><TR>\n",
              i + 1, config.labelTXT[i].c_str()  , batVolts[i], CHARGEMINUTES[(int)batVolts[i]], minutes, batteryCharged[i], batteryConnected[i]);
    } else {
      
      sprintf(tempstr, "<TR><TD> %d (%s) </TD><TD> %5.2f </TD> <TD> %d  </TD><TD> %d </TD><TD> %d </TD><TR>\n",
              i + 1, config.labelTXT[i].c_str()  ,batVolts[i], CHARGEMINUTES[(int)batVolts[i]], batteryCharged[i], batteryConnected[i]);
    }
    strcat(responseHTML, tempstr);
  }
  strcat(responseHTML, "</TABLE>");
  if (forceCharge) {
    sprintf(tempstr, "<H3>Charge Cycle Forced<h3>\n");
    strcat(responseHTML, tempstr);
  }
  if (allCharged) {
    sprintf(tempstr, "<H3>All Charged, delaying next charge cycle until needed, scan in %d minutes<h3>\n", (TESTCYCLE - (runMinutes % TESTCYCLE)));
    strcat(responseHTML, tempstr);
  } 
  strcat(responseHTML, "<A href=\"/forcescan\">Scan Batteries</A> <BR> <A href=\"/forcecharge\">Force Charge Cycle</A> <BR><A href=\"/editlabels\">Edit Battery Labels</A> <BR><A href=\"/connectAP\">Connect to Wifi Network using Wifi Manager</A><BR><A href=\"/serverIndex\">Update Firmware or Reboot</A> </body></html>\n");
  if (DEBUG)
    Serial.print(responseHTML);
  delay(100);// Serial.print(responseHTML);
  webServer.send(200, "text/html", responseHTML);

}

// Function to extract the new label text and update the labels array. The labels are then written to the labels.txt file
void handleForm() {
  float voltsExpected = -1;
  float voltsDisplayed = -1;
  
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "text/plain", "Method Not Allowed");
  } else {
    String message = "POST form was:\n";
    for (uint8_t i = 0; i < webServer.args(); i++) {
      String name = webServer.argName(i);
      if (name == "Expected") {
        String val = webServer.arg(i);
        voltsExpected  = val.toFloat();
      } else if (name == "Displayed") {
        String val = webServer.arg(i);
        voltsDisplayed  = val.toFloat();
      } else if (name == "mqttBroker") {
        config.mqttBroker = webServer.arg(i);
        config.mqttBroker.trim();
      } else if (name == "mqttPort") {
        String val = webServer.arg(i);
        config.mqttPort = val.toInt();   
      } else if (name == "mqttUser") {
        config.mqttUser = webServer.arg(i);
        config.mqttUser.trim();
      } else if (name == "mqttPasswd") {
        config.mqttPasswd = webServer.arg(i);
        config.mqttPasswd.trim();
      } else if (name == "mqttTopic") {
        config.mqttTopic = webServer.arg(i);
        config.mqttTopic.trim();
      } else {
        long whichlabel = name.toInt() -1;
        Serial.println(name + "" + whichlabel);
        if (whichlabel > -1 && whichlabel < RELAYS)
          config.labelTXT[whichlabel] = webServer.arg(i);
          config.labelTXT[whichlabel].trim();
        message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
      }
    }
    // Serial.print(message);
    Serial.printf("Expected: %f  Displayed: %f\n", voltsExpected, voltsDisplayed);
    // Recalculate the calibration constant based on the entered observed voltages
    // Empty or Alpha cells are ignored 
    if ( ( voltsExpected > 1 ) && ( voltsDisplayed > 1)){
      config.calConst = voltsExpected / (voltsDisplayed / config.calConst );
    }
    saveConfiguration(confFile,config);
    printFile(confFile);
    formPage();
  }
}

// Set flag for force Charge
void forceChargeCycle(){
  forceCharge = 1;
  webServer.send(200, "text/plain", "Press Back Button to return to list");
}

//Force a scan cycle
void forceScanCycle() {
  forceScan = 1;
  webServer.send(200, "text/plain", "Press Back Button to return to list");
}


void connectAP(){
  
  webServer.send(200, "text/html", "<html><body><A href=\"http://10.10.10.1\">Click to open WiFi Manager</A></body></html>"); 
  webServer.close();
  webServer.stop();
  delay(2000);
  
  mySmartConfig();
  
}







void webLoop(){
    webServer.handleClient();
}
