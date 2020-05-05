// file and diagnostic tools
// This function checks server args. If none serves base page,
// if there are, it produces the intended output:
// * file dir
// * file upload
// * Led control
// * diagnostics
void tools() {
  // No arguments were received
  if (server.args() == 0 ) {
    fs::File file = SPIFFS.open("/tools.html", "r");
    server.streamFile(file,"text/html");
    file.close();

  // file dir
  } else if (server.arg("f") == "dir") {
    // json object should look like this:
    // [{name : name, fileSize : size},{name : name, fileSize : size},{name : name, fileSize : size}...]
    String object = "[";
    File root = SD.open("/");
    if (root) {
      root.rewindDirectory();
      File file = root.openNextFile();
      while(file){
        object += "{ \"name\": \"" + String(file.name()) + "\", \"fileSize\": \"";
        int bytes = file.size();
        String fsize = "";
        if (bytes < 1024)                     fsize = String(bytes)+" B";
        else if(bytes < (1024 * 1024))        fsize = String(bytes/1024.0,3)+" KB";
        else if(bytes < (1024 * 1024 * 1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
        else                                  fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
        object += fsize+"\"}";
        file = root.openNextFile();
        if (file) object += ",";
      }
      file.close();
    }
    root.close();
    object += "]";
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send ( 200, "application/json", object);

  // download a file
  } else if (server.arg("f") == "dwn") {
    String filename = server.arg("file");
    File dataFile = SD.open(filename, FILE_READ); // Now read data from SD Card
    if (dataFile) {
      if (dataFile.available()) { // If data is available and present
        String dataType = "application/octet-stream";
        if (server.streamFile(dataFile, dataType) != dataFile.size()) {
          D(F("Sent less data than expected!")); }
      }
      dataFile.close(); // close the file:
    } else {
      server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      server.sendHeader("Pragma", "no-cache");
      server.sendHeader("Expires", "-1");
      server.send ( 500, "text/html", "Error - file not found!");
    }


  // delete a file
  } else if (server.arg("f") == "del") {
    String filename = server.arg("file");
    String response;
    int responseCode = 200;
    File dataFile = SD.open("/"+filename, FILE_READ); // Now read data from SD Card
    if (dataFile) {
      if (SD.remove("/"+filename)) {
        response = "OK - File deleted successfully";
      } else {
        responseCode = 500;
        response = "Error - File was not deleted!";
      }
    } else {
      responseCode = 500;
      response = "Error - File not found!";
    }
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send ( responseCode, "text/html", response);

  // chontrol the LED strip
  } else if (server.arg("f") == "leds") {
    fill_solid( leds, N_LEDS, CRGB(server.arg(1).toInt(), server.arg(2).toInt(), server.arg(3).toInt()));
    FastLED.show();
    D("pins received: " + server.arg(0) + "," + server.arg(1) + "," + server.arg(2) + "," + server.arg(3) + "\n");
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send ( 200, "text/html", "OK - LEDs changed");

  // output diagnostics
  } else if (server.arg("f") == "diag") {
    // return diagnostics summary
  }

}
