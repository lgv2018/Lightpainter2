/*
 *   Lightpainter2 v2.0
 *   LED strip driver for POV image painting
 *
 *   Copyright (c) 2020 Reven Sanchez
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// Debug flag
//#define DEBUG

// Preemtives
#define FASTLED_INTERNAL                 // prevents pragma notices
#define FASTLED_ESP8266_RAW_PIN_ORDER    // forces use of right pin nomenclature

// Includes
#include "debug.h"
#include "gamma.h"

// Libraries
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <FastLED.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <U8g2lib.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>

// Load sensitive configuration variables
#include "config.h"

// CONFIGURABLE STUFF --------------------------------------------------------
#define   servername "lightpainter2"     // Server name for mDNS
#define   SENSORNAME "Lightpainter2"     // Nice server name for other services
IPAddress local_IP(192, 168, 1, 150);    // Set your server's fixed IP address here
IPAddress gateway(192, 168, 1, 1);       // Set your network Gateway usually your Router base address
IPAddress subnet(255, 255, 255, 0);      // Set your network sub-network mask here
IPAddress dns(192,168,1,1);              // Set your network DNS usually your Router base address
#define   N_LEDS         144             // Max value was 170 because it fits one SD card block, but now we're seeking. So maybe more?

#define   SPI_SPEED      SD_SCK_MHZ(50)  // This might be optional. 50 is SD spec
#define   TRIGGER        A0              // Playback trigger pin
#define   SEL_DN         D1 // GPIO5     // Down pin
#define   SEL_UP         D2 // GPIO4     // Up pin
#define   I2C_DA         D3 // GPIO0     // I2C data pin
#define   I2C_CK         D4 // GPIO2     // I2C clock pin
//        SPI_CLOCK      D5 // GPIO14    // SPI default pins
//        MISO           D6 // GPIO12
//        MOSI           D7 // GPIO13
#define   SD_CS_PIN      D8 // GPIO3
#define   LED_PIN        D9 // GPIO15    // IO3 - WS2812 strip connects here
#define   CURRENT_MAX  3000              // Max current from power supply (mA)
#define   LED_RED         0              // Component order of the LED strip. WS2812 is sometimes GRB
#define   LED_GREEN       1              // (others may vary, generally RGB)
#define   LED_BLUE        2

// OTA config
#define  OTAport     8266

// NON-CONFIGURABLE STUFF ----------------------------------------------------
// Constants
const uint8_t     configVersion = 34;  // v flag, safeguard for EEPROM read config
#define           BMP_BLUE       0     // Component order of BMP files. BMP are BGR
#define           BMP_GREEN      1
#define           BMP_RED        2

// Global vars
uint8_t           nImages = 0,         // Total # of image files
                  cImage  = 0,         // Current image # selected
                  menuState,           // Current page of menu to display
                  selected,            // A flag for tracking selected menu option
                  readBuf[512];        // Buffer for reading rows of image
CRGB              leds[N_LEDS];        // Reserve memory for LED strip
char              fileIndex[50][25];   // Array of file indexes. This uses 1250bytes of RAM.
int16_t           scrollPos;           // scroll position for wrapping text on OLED

// Input/debouncing vars
uint8_t           trigger = 0;
uint8_t           UpState = 0;
uint8_t           DnState = 0;
uint8_t           Rtrigger = 0;
uint8_t           RUpState = 0;
uint8_t           RDnState = 0;
uint8_t           prevTrigger = 0;
uint8_t           prevUpState = 0;
uint8_t           prevDnState = 0;
uint8_t           longUp = 0;
uint8_t           longDn = 0;
unsigned long     lastUp = 0;
unsigned long     lastDn = 0;
unsigned long     lastTrigger = 0;
unsigned long     debounceDelay = 50;  // the debounce time; increase if the output flickers


// counters and time
unsigned long     currentMillis = 0;   // keeps current millis() in each loop
unsigned long     timePast = 0;        // generic timer
unsigned long     scrollTime = 0;      // tracks a scroll of the OLED

// Default light show settings. These are overwritten by EEPROM saved values if they exist
uint8_t           ledBrightness = 125, // Adjustable brightness
                  Speed = 125,         // Adjustable speed
                  Delay = 0;           // Adjustable initial delay in sec

File              UploadFile;          // File handler for uploads

// Initialize OLED screen instance
U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ I2C_CK, /* data=*/ I2C_DA);   // pin remapping with ESP8266 HW I2C

// WiFi credentials. Are set interactively through interface
char              ssid[32] = "";
char              password[32] = "";
bool              okCred = true; // flag to check if we have wifi credentials.

// Declare servers and services
const uint8_t     DNS_PORT = 53;
DNSServer         dnsServer;            // Initialize the DNS server
ESP8266WebServer  server(80);           // Initialize the web server
WiFiClient        espClient;            // Initialize the WiFiClient
IPAddress         apIP(192, 168, 4, 1); // Soft AP network parameters
IPAddress         netMsk(255, 255, 255, 0);
long              lastConnectTry = 0;   // Time since last WLAN connection atempt

// additional tools
#ifdef DEBUG
  #include "tools.h"
#endif


// SETUP -----------------------------------------------------------------------

void setup(){
  #ifdef DEBUG
  Serial.begin(115200);
  delay(1000); // avoid dumping garbage
  D(F("\n** Lightpainter2 debug info **\n"));
  #endif

  // Setup pins and interrupts -- needs some work!!
  //pinMode(TRIGGER, INPUT_PULLUP);
  pinMode(SEL_UP, INPUT_PULLUP);
  pinMode(SEL_DN, INPUT_PULLUP);

  // Load lightshow config from EEPROM
  loadConfig();

  // Setup LEDs
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, N_LEDS).setCorrection( TypicalLEDStrip ); // check rgb vs grb
  FastLED.setBrightness(ledBrightness);
  FastLED.clear();
  FastLED.show();                          // Initialize all pixels to 'off'

  // Configure and connect to WiFi
  loadCredentials(); // Load WLAN credentials from EEPROM
  if (!WiFi.config(local_IP, gateway, subnet, dns)) {
    D(F("Failed to load WiFi parameters correctly!\n"));
    okCred = 0;
  }
  wifiConnect();

  // Start the softAP
  apConnect();

  // Start OTA
  ArduinoOTA.setPort(OTAport);
  ArduinoOTA.setHostname(SENSORNAME);
  ArduinoOTA.setPassword((const char *)OTApassword);
  D(F("Starting *OTA* Node named "));
  D(SENSORNAME);
  D(F("\n"));

  // OTA control
  ArduinoOTA.onStart([]() {
    D(F("Starting"));
  });
  ArduinoOTA.onEnd([]() {
    D(F("\nEnd"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    D(F("Progress: "));
    D(progress / (total / 100));
    D(F("%\n"));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    D(F("Error "));
    D(error);
    D(F(": "));
    if (error == OTA_AUTH_ERROR) D(F("Auth Failed\n"));
    else if (error == OTA_BEGIN_ERROR) D(F("Begin Failed\n"));
    else if (error == OTA_CONNECT_ERROR) D(F("Connect Failed\n"));
    else if (error == OTA_RECEIVE_ERROR) D(F("Receive Failed\n"));
    else if (error == OTA_END_ERROR) D(F("End Failed\n"));
  });
  ArduinoOTA.begin();
  D(F("OTA Ready\n"));

  // Start DNS for softAP, redirects all domains to the apIP
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  // mDNS
  // OTA should've loaded mDNS module, so we should only need to add a port
  MDNS.addService("http", "tcp", 80);

  // Start SD
  D(F("Initializing SD card..."));
  // see if the card is present and can be initialised.
  if (!SD.begin(SD_CS_PIN, SPI_SPEED)) {
    D(F("Card failed or not present, not able to continue...\n"));
  } else {
    D(F("Card initialised... file access enabled...\n"));
    scan(); // populate the fileIndex
  }

  // Start SPIFFS
  if (!SPIFFS.begin()) {
    D("SPIFFS initialisation failed...\n");
  } else {
    D(F("SPIFFS initialised... file access enabled...\n"));
  }

  // Start OLED
  u8g2.begin();

  // Start the webserver and define pages
  server.on ( "/",            handleRoot);
  server.on ( "/image",       handleImage );
  server.on ( "/getImageTot", sendImageTot );
  server.on ( "/getImageNum", sendImageNum );
  server.on ( "/getImage",    getImage );
  server.on ( "/setImage",    setImage );
  server.on ( "/start",       handleStart );
  server.on ( "/wifi",        handleWifi );
  server.on ( "/getWifi",     handleGetWifi );
  server.on ( "/wifisave",    handleWifiSave );
  server.on ( "/change",      webChangeSettings );
  server.on ( "/settings",    handleSettings );
  server.on ( "/save",        webSaveSettings );
  server.on ( "/getSettings", handleGetSettings );
  server.on ( "/startShow",   webStartShow );
  server.on ( "/iupload",     HTTP_POST,[](){ server.send(200);}, handleFileUpload );
  #ifdef DEBUG
  server.on ( "/tools",       tools );
  #endif
  server.onNotFound ( handleNotFound );
  server.begin();
  D(F("HTTP Server up and running\n"));

  // reset LEDs again just in case there's any stuck pixel
  FastLED.clear();
  FastLED.show();

} // end setup


// MENU & PLAYBACK LOOP -------------------------------------------------------------
// Draw the menu, listen for input, change flags based on input and keep services up.
void loop(){
  // Track time each loop for non-blocking functions
  currentMillis = millis();

  // OTA check
  ArduinoOTA.handle();

  // Display the menu. // potentially limit this function
  u8g2.firstPage();
  do {
    menuDisplay();
  } while ( u8g2.nextPage() );

  // Listen for client connections
  server.handleClient();

  // Get Input from user
  getInput();

} // end loop


// --------------------------------------------------------------------------#
//     * FUNCTIONS ----------------------------------------------------------#
// --------------------------------------------------------------------------#
//     - SERVER HELPER
//     - SD SCANNING
//     - MENU INTERFACE
//     - PAINTING FUNCTION
//     - EEPROM HANDLING
//     - WEB SERVER FUNCTIONS


// SERVER HELPER -------------------------------------------------------------

// connect to wifi and establish wifi mode based on network access
void wifiConnect() {
  delay(10);

  if (okCred == false) { // no credentials, we can only load an AP
    apConnect();

  } else { // we've got credentials, let's try to connect
    D(F("Connecting to network "));
    D(ssid);
    D("\n");

    // track time
    currentMillis = millis();

    WiFi.hostname(SENSORNAME);
    WiFi.begin(ssid,password);

    // try to connect for 10 sec
    while (( WiFi.status() != WL_CONNECTED ) && ( millis() - currentMillis < 10000 )) {
    delay(500);
    D(".");
    }

    // check connection
    if (WiFi.status() == WL_CONNECTED) {
      // Debug. Report which SSID and IP is in use
      D(F("\nWiFi connected\nIP address: "));
      D(WiFi.localIP());
      D(F("\n"));

    } else {
      // Fall back to AP
      D(F("\nCould not connect; falling back to AP\n"));
    }
  }
}

// create SoftAP
void apConnect() {
  delay(1000); // just make sure connection is active
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(SENSORNAME, softAP_password);
  MDNS.notifyAPChange();
  D(F("Starting SoftAP named "));
  D(SENSORNAME);
  D(F("\n"));
  delay(500);
}

// test if string is an IP
boolean isIp(String str) {
  for (byte i = 0; i < str.length(); i++) {
    byte c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

// convert IP to String
String toStringIp(IPAddress ip) {
  String res = "";
  for (byte i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

//Returns a human readable file size
String file_size(int bytes){
  String fsize = "";
  if (bytes < 1024)                 fsize = String(bytes)+" B";
  else if(bytes < (1024*1024))      fsize = String(bytes/1024.0,3)+" KB";
  else if(bytes < (1024*1024*1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
  else                              fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
  return fsize;
}

// Redirect to captive portal if we got a request for another domain. Return true
//  in that case so the page handler doesn't try to handle the request again.
boolean captivePortal() {
  if (!isIp(server.hostHeader()) && server.hostHeader() != (String(servername) + ".local")) {
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send ( 302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}


// SD SCANNING ---------------------------------------------------------------

// Scans SD to get number of pictures (nImages) and populates fileIndex.
void scan() {
  D(F("Scanning files...\n"));
  File root = SD.open("/");
  root.rewindDirectory();
  File file = root.openNextFile();
  while(file){
    if (!file.isDirectory() && strncmp(file.name(),".",1) != 0) { // if it's not a dir or .(dot) file
      if (strstr(file.name(),"bmp") > 0 ) { // only bmp
        // copy to file Index array
        strcpy(fileIndex[nImages],file.name());
        D(F("\t found "));
        D(file.name());
        D(F("\n"));
        nImages++;
      }
    }
    file = root.openNextFile();
  }
  file.close();
  D(F("Done scanning! :: Images: "));
  D(nImages);
  D(F("\n"));
}


// MENU INTERFACE ------------------------------------------------------------

// The menu draw functions are in a separate file
#include "menu128x32.h"

// Display the menu
void menuDisplay() {
  switch(menuState) {
    case 0: title(); break;          // Intro screen
    case 1: menu_start(); break;     // Main screen
    case 2: menu_main(); break;      // Menu
    case 3: select_file(); break;    // Select file interface
    case 4: select_level(); break;   // Select level interface for brightness, speed and delay
    case 5: save_settings(); break;  // Confirmation of settings saved
  }
}

// Get the input. Reads buttons and pushes actionInput if it must
void getInput() {

  // Read trigger
  Rtrigger = ( analogRead(TRIGGER) > 1000 ) ? 1 : 0 ; // we shouldnt be reading adc so quickly
  if (Rtrigger != prevTrigger) {
   lastTrigger = currentMillis;
  }
  if (currentMillis - lastTrigger > debounceDelay ) {
    if (Rtrigger != trigger) {
      trigger = Rtrigger;
      D(F("<<IN>> Trigger: "));
      D(trigger);
      D(F("\n"));
      if (trigger == 1) actionInput();
      delay(50);
    }
  }
  prevTrigger = Rtrigger; // save state for next loop


  // Read Up button
  RUpState = 1 - digitalRead(SEL_UP);
  if (RUpState != prevUpState) {
    lastUp = currentMillis;
  }
  if (currentMillis - lastUp > debounceDelay ) {
    if (RUpState != UpState) {
      UpState = RUpState;
      longUp = 0;
      D(F("<<IN>> Up: "));
      D(UpState);
      D(F("\n"));
      if (UpState == 1) actionInput();
      delay(50);
    } else if (UpState == 1 && currentMillis - lastUp > 500) {
      longUp = 1; actionInput();
    } else if (UpState == 1 && currentMillis - lastUp > 1500) {
      longUp = 1; actionInput();
    }
  }
  prevUpState = RUpState; // save state for next loop


  // Read Dn button
  RDnState = 1 - digitalRead(SEL_DN);
  if (RDnState != prevDnState) {
    lastDn = currentMillis;
  }
  if (currentMillis - lastDn > debounceDelay ) {
    if (RDnState != DnState) {
      DnState = RDnState;
      longDn = 0;
      D(F("<<IN>> Dn: "));
      D(DnState);
      D(F("\n"));
      if (DnState == 1) actionInput();
      delay(50);
    } else if (DnState == 1 && currentMillis - lastDn > 500) {
      longDn = 1; actionInput();
    } else if (DnState == 1 && currentMillis - lastDn > 1500) {
      longDn = 2; actionInput();
    }
  }
  prevDnState = RDnState; // save state for next loop

}
// Act on input. Modifies menu structure and flags based on user input
void actionInput() {
  switch (menuState) {
    case 0: // title screen; any button moves forward. No need to test
      menuState = 1;
      break;

    case 1: // main screen
      if (selected == 0 && UpState > 0) selected = 1;
      if (selected == 1 && DnState > 0) selected = 0;
      if (selected == 0 && trigger > 0) showImage(cImage);
      if (selected == 1 && trigger > 0) {selected = 2; menuState = 2;}
      break;

    case 2: // main menu
      if (UpState > 0) {
        selected++;
        if (selected > 6) selected = 6;
      }
      if (DnState > 0) {
        selected--;
        if (selected < 2) selected = 2;
      }
      if (trigger > 0) {
        if (selected > 5){
          saveConfig();  // save to EEPROM
          menuState = 5;
          selected = 0;
        }
        if (selected > 2){
          menuState = 4;
        }
        if (selected == 2){
          menuState = 3;
        }
      }
      break;

    case 3: // file selection
      if (UpState > 0 && cImage > 0) cImage--;
      if (DnState > 0 && cImage < nImages - 1) cImage++;
      if (trigger > 0) { menuState = 1; selected = 0; }
      break;

    case 4: // adjust brightness, speed or delay
      uint8_t * ptr; // adjust values by reference to use same structure
      if (selected == 3) {ptr = &ledBrightness;}
      if (selected == 4) {ptr = &Speed;}
      if (selected == 5) {ptr = &Delay;}

      if (UpState > 0 && *ptr < 255) { *ptr=*ptr+1;}
      if (longUp == 1 && *ptr < 251) { *ptr=*ptr+5;}
      if (longUp == 2 && *ptr < 230) { *ptr=*ptr+25;}
      if (DnState > 0 && *ptr > 0)   { *ptr=*ptr-1;}
      if (longDn == 1 && *ptr > 4)   { *ptr=*ptr-5;}
      if (longDn == 2 && *ptr > 24)  { *ptr=*ptr-25;}
      if (trigger > 0){
        menuState = 1;
        selected = 0;
      }
      break;

    case 5: // confirmation; any key passes. No need to test
      menuState = 1;
      break;

  }
  delay(300);
}


// PAINTING FUNCTION ---------------------------------------------------------

// Reads BMP file, does a safety brightness check, then loads the file row by row,
// interpolates RGB values with gamma array and pushes values into leds[] array.
boolean showImage( uint8_t index ){

  File      image;               // Windows BMP file for input
  boolean   flip      = false;   // 'true' if image stored top-to-bottom
  int       row,                 // Current image row (Y)
            bmpHeight;           // BMP height in pixels
  uint8_t   bmpWidth,            // BMP width in pixels
            bmpStartCol,         // First BMP column to process (crop/center)
            columns,             // Number of columns to process (crop/center)
            column,              // and column (X)
            color,               // Color component index (R/G/B)
            raw,                 // 'Raw' R/G/B color value
            corr,                // Gamma-corrected R/G/B
            pixel[3],            // For reordering color data, BGR to GRB
           *ledPtr,              // Pointer into readBuf (output)
           *ledStartPtr;         // First LED column to process (crop/center)
  uint32_t  bmpImageoffset,      // Start of image data in BMP file
            rowSize,             // BMP row size (bytes) w/32-bit alignment
            lineMax = 0L,        // Cumulative brightness of brightest line
            sum;                 // Sum of pixels in row

  D(F("=>Starting show for "));
  D( fileIndex[index] );
  D(F("\n"));
  image = SD.open(fileIndex[index], FILE_READ);
  if(!image) {
    D(F("Failed :( can't open file"));
    return false;
  }

  // Validate bitmap format
  if(image.read(readBuf, 34)              &&    // Load header
    (*(uint16_t *)&readBuf[ 0] == 0x4D42) &&    // BMP signature
    (*(uint16_t *)&readBuf[26] == 1)      &&    // Planes: must be 1
    (*(uint16_t *)&readBuf[28] == 24)     &&    // Bits per pixel: must be 24
    (*(uint32_t *)&readBuf[30] == 0)) {         // Compression: must be 0 (none)
    // Supported BMP format -- proceed!
    bmpImageoffset = *(uint32_t *)&readBuf[10]; // Start of image data
    bmpWidth       = *(uint32_t *)&readBuf[18]; // Image dimensions
    bmpHeight      = *(uint32_t *)&readBuf[22];

    rowSize = ((bmpWidth * 3) + 3) & ~3; // 32-bit line boundary

    if(bmpHeight < 0) {       // If bmpHeight is negative,
      bmpHeight = -bmpHeight; // image is in top-down order.
      flip      = true;       // Rare, but happens.
    }

    if(bmpWidth >= N_LEDS) { // BMP matches LED bar width, or crop image
      bmpStartCol = (bmpWidth - N_LEDS) / 2;
      ledStartPtr = readBuf;
      columns     = N_LEDS;
    } else {                 // Center narrow image within LED bar
      bmpStartCol = 0;
      ledStartPtr = &readBuf[((N_LEDS - bmpWidth) / 2) * 3];
      columns     = bmpWidth;
      memset(readBuf, 0, N_LEDS * 3); // Clear left/right pixels
    }

    // Double check brightness first. This quick traverse only costs us about 180ms
    // Good investment in security!!
    long unsigned startTime = millis();
    for(row=0; row<bmpHeight; row++) { // For each row in image...

      // Seek to first pixel to load for this row...
      image.seek(
        bmpImageoffset + (bmpStartCol * 3) + (rowSize * (flip ?
        (bmpHeight - 1 - row) : // Image is stored top-to-bottom
        row)));                 // Image stored bottom-to-top
      if(!image.read(ledStartPtr, columns * 3))  // Load row
        D(F(":( Error reading file for check"));

      sum       = 0L;                               // Sum of pixel values
      for(column=0; column<columns; column++) {     // For each column...
        raw  = ledStartPtr[column];                 // get raw value...
        corr = pgm_read_byte(&gamma8[raw]);         // ...gamma-corrected
        sum      += corr;                           // ...and add to total brightness
      } // Next column
      if(sum > lineMax) lineMax = sum;
    } // Next row

    // DEBUG
    D(F("\tBrightness processed in "));
    D(millis() - startTime);
    D(F(" ms; linemaxraw is "));
    D(lineMax);
    D(F("\n"));

    // Now adjust brightness
    lineMax = (lineMax * 20) / 255;             // Est. current; a 255 value would be ~20 mA/LED
    lineMax = (lineMax * ledBrightness ) / 255; // And adjust for current brightness
    if (lineMax > CURRENT_MAX ) {
      FastLED.setBrightness( CURRENT_MAX * ledBrightness / lineMax);
      D(F("\tBrightness adjusted. lineMax was "));
      D(lineMax);
      D(F("\n"));
    } else {
      D(F("\tBrightness OK. lineMax was "));
      D(lineMax);
      D(F("\n"));
    }

    // Calculate our delay based on Speed
      // Changed to linear scale.
      // 255 = no delay ( about 12ms per cycle, for a total of 2000 to 3000 ms per image)
      // 127 = half delay, around 15s per image, delay of about 45.
      // 0   = slow, around 30s per image, delay of 95
      //  y=-0.36x+92 though these values will vary with hardware
    uint8_t stepDelay = round ( -0.363 * Speed + 92.4 );
    D(F("\tStep delay calculated at "));
    D(stepDelay);
    D(F("\n"));

    // Ready to start show!
    // Dim display and do the start delay.
    u8g2.clear();
    delay ( Delay * 1000 );

    // now we actually show the image
    // If trigger is pressed loop image
    uint8_t repeat = 1;
    while (repeat == 1 ) {
    startTime = millis();

    // Start loading image
    for(row=0; row<bmpHeight; row++) { // For each row in image...
      // Seek to first pixel to load for this row...
      image.seek(
        bmpImageoffset + (bmpStartCol * 3) + (rowSize * (flip ?
        (bmpHeight - 1 - row) : // Image is stored top-to-bottom
        row)));                 // Image stored bottom-to-top
      if(!image.read(ledStartPtr, columns * 3))  // Load row
        D(F(":( Error reading file for output"));

      ledPtr    = ledStartPtr;  // pass pointer to buffer

      for(column=0; column<columns; column++) {   // For each column...
        // reorder R/G/B
        pixel[LED_BLUE]  = ledPtr[BMP_BLUE];
        pixel[LED_GREEN] = ledPtr[BMP_GREEN];
        pixel[LED_RED]   = ledPtr[BMP_RED];

        for(color=0; color<3; color++) {              // 3 color bytes...
          raw  = pixel[color];                        // 'Raw' G/R/B
          corr = pgm_read_byte(&gamma8[raw]);         // Gamma-corrected
          leds[column][color] = corr;                 // Here we store the corrected version in leds[]

          *ledPtr++;                                  // and increase ledPtr pointer

        } // Next color byte
      } // Next column

      // done with row, so push it out
      FastLED.show();
      FastLED.delay(stepDelay); // based on Speed

    } // Next row

    // DEBUG
    D(F("\tImage displayed in "));
    D(millis() - startTime);
    D(F(" ms"));
    D(F("\n\twith a speed value of "));
    D(Speed);
    D(F("\n"));

    if (analogRead(TRIGGER) < 300) repeat = 0;

    } // end of while trigger pressed loop

    // Done! Restore brightness and turn off strip
    D(F("\t:) show done!\n"));
    memset(leds, 0, N_LEDS * 3);           // Clear LED buffer
    FastLED.setBrightness(ledBrightness);  // Restore brightness
    FastLED.show();                        // Init LEDs to 'off' state
    FastLED.delay(10);                     // To give strip an opportunity to stop

  } else { // end BMP header check
    D(F(":( BMP format not recognized."));
  }

  image.close();
  return true; // 'false' on various file open/create errors
}


// EEPROM HANDLING -----------------------------------------------------------

// EEPROM map:          0 -> ssid
//                     32 -> password
//                     64 -> ok
//                     70 -> config serial flag (configVersion);
//                     71 -> brightness
//                     72 -> speed
//                     73 -> delay

// Saves config values to EEPROM.
void saveConfig() {
  EEPROM.begin(512);
  EEPROM.put(70,configVersion);
  EEPROM.put(71,ledBrightness);
  EEPROM.put(72,Speed);
  EEPROM.put(73,Delay);
  EEPROM.commit();
  EEPROM.end();
}

// Loads configuration variables if position 0 contains the right flag for
// this version ( configVersion ).
void loadConfig() {
  uint8_t flag;
  EEPROM.begin(512);
  EEPROM.get(70, flag);
  if (flag == configVersion){
    EEPROM.get(71, ledBrightness);
    EEPROM.get(72, Speed);
    EEPROM.get(73, Delay);
  }else{
    D("INFO: EEPROM mismatch. Not loading values\n");
  }
  EEPROM.end();
}

// Load WLAN credentials from EEPROM
void loadCredentials() {
  EEPROM.begin(512);
  EEPROM.get(0, ssid);
  EEPROM.get(0 + sizeof(ssid), password);
  char ok[2 + 1];
  EEPROM.get(0 + sizeof(ssid) + sizeof(password), ok);
  EEPROM.end();
  if (String(ok) != String("OK")) {
    ssid[0] = 0;
    password[0] = 0;
  }
}

// Store WLAN credentials to EEPROM
void saveCredentials() {
  EEPROM.begin(512);
  EEPROM.put(0, ssid);
  EEPROM.put(0 + sizeof(ssid), password);
  char ok[2 + 1] = "OK";
  EEPROM.put(0 + sizeof(ssid) + sizeof(password), ok);
  EEPROM.commit();
  EEPROM.end();
}


// WEB SERVER FUNCTIONS ------------------------------------------------------

void handleRoot(){

  // If captive portal, redirect instead of displaying the page.
  if (captivePortal()) {
    return;
  }
  if (server.client().localIP() == apIP) { // We are connected through the softAP
    fs::File file = SPIFFS.open("/landing.html", "r");
    size_t sent = server.streamFile(file,"text/html");
    if (!file) D(F("There has been an error reading the landing page!!\n"));
    file.close();
  } else { // We're connected through wifi
    fs::File file = SPIFFS.open("/start.html", "r");
    size_t sent = server.streamFile(file,"text/html");
    file.close();
  }
}


// Any other pages we don't know
void handleNotFound() {
  if (captivePortal()) { // If captive portal redirect instead of displaying the error page.
    return;
  }
  // check if it's in SPIFFS
  if ( !loadFromSpiffs(server.uri()) ) {
    D(F("File Not Found\n"));
    D(server.uri());
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for ( byte i = 0; i < server.args(); i++ ) {
      message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
    }
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send ( 404, "text/plain", message );
  }
}

// Read file from SPIFFS and send it to the client
bool loadFromSpiffs(String path) {
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    server.streamFile(file, contentType);               // And send it to the client
    file.close();                                       // Then close the file again
    D(F("Loaded from SPIFFS: "));
    D(path);
    D(F("\n"));
    return true;
  }
  return false;                                         // If the file doesn't exist, return false
}

// Define the MIME type of the file based on extension  --> Is there a quicker way than to test one by one?
String getContentType(String filename){
  if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".bmp")) return "image/bmp";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

// Send image management page
void handleImage() {
  fs::File file = SPIFFS.open("/image.html", "r");
  server.streamFile(file,"text/html");
  file.close();
}

// Send number of images
void sendImageTot() {
  server.send (200, "text/plain", String(nImages));
}

// Send current selected image
void sendImageNum() {
  server.send (200, "text/plain", String(cImage));
}

// Get an image from SD
void getImage() {
  uint8_t index = server.arg(0).toInt();
  D(F("Streaming file with index: "));
  D(index);
  D(F("\n"));
  File dataFile = SD.open(fileIndex[index], FILE_READ); // Now read data from SD Card
  if (dataFile) {
    if (dataFile.available()) { // If data is available and present
      String dataType = "application/octet-stream";
      if (server.streamFile(dataFile, dataType) != dataFile.size()) {
        D(F("Sent less data than expected!n")); }
    }
    dataFile.close(); // close the file:
  } else {
    D(F("Error - File could not be open on SPIFFS"));
    server.send(500, "text/plain" "Error - File could not be open on SPIFFS");
  }
}

// Set the selected image
void setImage() {
  server.send (200, "text/plain", "OK - Image set");
  cImage = server.arg(0).toInt();
}

// Send start page
void handleStart() {
  fs::File file = SPIFFS.open("/start.html", "r");
  server.streamFile(file,"text/html");
  file.close();
}

// Wifi config page handler
void handleWifi() {
  fs::File file = SPIFFS.open("/wifi.html", "r");
  server.streamFile(file,"text/html");
  file.close();
}

// Send wifi data to server
// This abuses String a bit, should optimize
void handleGetWifi() {
  String object = "{\"type\": ";
  if (server.client().localIP() == apIP) {
    object += "0, ";
  } else {
    object += "1, ";
  }
  object += "\"ap_ssid\": \"" + String(SENSORNAME) + "\", ";
  object += "\"wan_ssid\": \"" + String(ssid) + "\", ";
  object += "\"ap_ip\": \"" + toStringIp(WiFi.softAPIP()) + "\", ";
  object += "\"wan_ip\": \"" + toStringIp(WiFi.localIP()) + "\", ";

  int n = WiFi.scanNetworks();
  if (n > 0) {
    object += "\"networks\": [";
    for (int i = 0; i < n; i++) {
      object += "{ \"name\": \"" + WiFi.SSID(i) + "\", \"enc\": " + String((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? 0 : 1) + ", \"rssi\": \"" + WiFi.RSSI(i) + "\" }";
      if (i < n-1) object += ", ";
    }
    object += " ]";
  } else {
    object += "\"networks\": 0";
  }
  object += "}";

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send ( 200, "application/json", object);

}

// Handle the WLAN save form and redirect to WLAN config page again
void handleWifiSave() {
  D(F("Receiving form with wifi connection info\n"));
  server.arg("n").toCharArray(ssid, sizeof(ssid) - 1);
  server.arg("p").toCharArray(password, sizeof(password) - 1);
  D(F("SSID: "));
  D(String(ssid));
  D(F(" ; PASS: "));
  D(String(password));
  D(F("\n"));
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send ( 200, "text/plain", "OK - Form recieved correctly");
  saveCredentials();
  wifiConnect();
}

// Send settings page
void handleSettings() {
  fs::File file = SPIFFS.open("/settings.html", "r");
  size_t sent = server.streamFile(file,"text/html");
  file.close();
}

// Send settings as json
void handleGetSettings() {
  String json = String("{ \"bri\": " + String(ledBrightness) + ", \"spe\": " + String(Speed) + ", \"del\": "\
  + String(Delay) + "}");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send ( 200, "application/json", json);
}

// Changes settings from web
void webChangeSettings() {
  if ( server.argName(0) == "bri") {
    ledBrightness = server.arg(0).toInt();
    server.send (200, "text/plain", "OK - Brightness updated");
  } else if ( server.argName(0) == "spe") {
    Speed = server.arg(0).toInt();
    server.send (200, "text/plain", "OK - Speed updated");
  } else if ( server.argName(0) == "del") {
    Delay = server.arg(0).toInt();
    server.send (200, "text/plain", "OK - Delay updated");
  }
}

// Web request to save settings to EEPROM
void webSaveSettings() {
  saveConfig();
  server.send (200, "text/plain", "OK - Settings saved to EEPROM");
}

// Web request to start show
void webStartShow() {
  showImage(cImage);
  server.send (200, "text/plain", "OK - Show done!");
}

// Upload an image to the SD
void handleFileUpload(){
  D(F("r"));
  HTTPUpload& uploadfile = server.upload();
  if(uploadfile.status == UPLOAD_FILE_START) {
    D(F("-init-"));
    String filename = uploadfile.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    D(F("Upload File Name: ")); D(filename); D(F("\n"));
    SD.remove(filename);                         // Remove a previous version, otherwise data is appended to the file
    UploadFile = SD.open(filename, FILE_WRITE);  // Open the file for writing
    filename = String();
  } else if (uploadfile.status == UPLOAD_FILE_WRITE) {
    D(F("w"));
    if(UploadFile) UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
  } else if (uploadfile.status == UPLOAD_FILE_END) {
    if(UploadFile) {  // If the file was successfully created
      UploadFile.close();   // Close the file again
      D(F("\n Finished! Upload Size: ")); D(uploadfile.totalSize); D(F("\n"));
      server.send(200,"text/html","OK - File uploaded correctly");
    } else {
      D(F("\nError! File could not be created.\n"));
      server.send(500,"text/html","Error! - The file could not be created");
    }
  }
}
