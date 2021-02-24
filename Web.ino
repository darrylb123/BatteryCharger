// Various functions to build the web pages
void initialiseWebUI(){
  // Allocate web page memory
  responseHTML = (char *)malloc(15000);
  webServer.on("/", handlePage);
  webServer.on("/postform/", handleForm);
  webServer.on("/rmfiles", rmfiles);
  webServer.on("/labels", labels);
  webServer.on("/editlabels", formPage);
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

  sprintf(tempstr, "<H3>Time since boot %d minutes %d hours %d days</H3>\n", runMinutes, runHours, runDays);
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
    sprintf(tempstr, "<H3>All Charged, delaying next charge cycle until needed, scan in %d minutes<h3>\n", (60 - (runMinutes % 60)));
    strcat(responseHTML, tempstr);
  } 
  strcat(responseHTML, "<A href=\"/editlabels\">Edit Battery Labels</A> </body></html>\n");
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
    webServer.send(200, "text/plain", message);
    Serial.print(message);
    writeFile();
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
