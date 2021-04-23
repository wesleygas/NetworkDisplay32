#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include "ESPAsyncWebServer.h"

WiFiMulti wifiMulti;

AsyncWebServer server(80);

HTTPClient http;
#define MAX_IMG_SIZE 32000
uint8_t imgBuff[MAX_IMG_SIZE];
uint32_t imgSize=0;

//flag to use from web update to reboot the ESP
bool shouldReboot = false;

#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library


/* ESP32 PINOUT

LED   ----  5V
SCK   ----  18
SDA   ----  23
A0    ----  2
RESET ----  4
CS    ----  15
GND   ----  GND
VCC   ----  3V3

*/


// JPEG decoder library
#include <JPEGDecoder.h>

// Return the minimum of two values a and b
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

void loadImageFromWeb(void);
void drawArrayJpeg(const uint8_t arrayname[], uint32_t array_size, int xpos, int ypos);

bool reqValid = 1;

void handleUploadImage(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //loadImageFromWeb(); replace with easy's logic
  if(index == 0){
    reqValid = 1;
    // Serial.print("UpImg url: ");
    // Serial.println(request->url());
    int contentSize = request->contentLength();
    if(contentSize){ 
      if(contentSize > MAX_IMG_SIZE){
        request->send(HTTP_CODE_PAYLOAD_TOO_LARGE,"text/plain",
        "MaxSize: " + String(MAX_IMG_SIZE) + " Got: " + String(contentSize));
        reqValid = 0;
      } 
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
    }else{
      request->send(HTTP_CODE_BAD_REQUEST);
      reqValid = 0;
    }
  }
  if(reqValid){
    int stopOn = index+len;
    Serial.printf("package incoming from %d to %d...",index,stopOn);
    for(int i = 0; i < len; i++){
      imgBuff[index+i] = data[i];
      //Serial.printf("%x, ",imgBuff[index+i]);
    }
    Serial.print("EOP\n");
    
    if(stopOn == total){
        Serial.print("\nFinal pack received\n");
        imgSize = total;
        tft.setRotation(0);  // portrait
        drawArrayJpeg(imgBuff, imgSize, 0, 0); // Draw a jpeg image stored in memory at x,y
        request->send(HTTP_CODE_OK);
    }
  }
}


void handleMeta(AsyncWebServerRequest *request){
  if(request->hasParam("s")){
    String message = request->getParam("s")->value();
    tft.fillRect(0,128,128,32,TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextWrap(true);
    tft.setCursor(0,128);
    tft.print(message);
    Serial.print("Got meta: ");
    Serial.println(message);
    request->send(HTTP_CODE_OK, "text/plain", message);
  }
  request->send(HTTP_CODE_BAD_REQUEST);
}

void handleRoot(AsyncWebServerRequest *request) {
  request->send(HTTP_CODE_OK, "text/plain", "Sup bro!");
}

void handleNotFound(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(HTTP_CODE_NOT_FOUND);
}



//####################################################################################################
// Draw a JPEG on the TFT, images will be cropped on the right/bottom sides if they do not fit
//####################################################################################################
// This function assumes xpos,ypos is a valid screen coordinate. For convenience images that do not
// fit totally on the screen are cropped to the nearest MCU size and may leave right/bottom borders.
void renderJPEG(int xpos, int ypos) {

  // retrieve infomration about the image
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
  uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // read each MCU block until there are no more
  while (JpegDec.readSwappedBytes()) {
	  
    // save a pointer to the image block
    pImg = JpegDec.pImage ;

    // calculate where the image block should be drawn on the screen
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;  // Calculate coordinates of top left corner of current MCU
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
    {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++)
      {
        p += mcu_w;
        for (int w = 0; w < win_w; w++)
        {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // draw image MCU block only if it will fit on the screen
    if (( mcu_x + win_w ) <= tft.width() && ( mcu_y + win_h ) <= tft.height())
    {
      tft.pushRect(mcu_x, mcu_y, win_w, win_h, pImg);
    }
    else if ( (mcu_y + win_h) >= tft.height()) JpegDec.abort(); // Image has run off bottom of screen so abort decoding
  }

  // calculate how long it took to draw the image
  drawTime = millis() - drawTime;

  // print the results to the serial port
  Serial.print(F(  "Total render time was    : ")); Serial.print(drawTime); Serial.println(F(" ms"));
  Serial.println(F(""));
}



//####################################################################################################
// Draw a JPEG on the TFT pulled from an array
//####################################################################################################
void drawArrayJpeg(const uint8_t arrayname[], uint32_t array_size, int xpos, int ypos) {

  int x = xpos;
  int y = ypos;

  int retcode = JpegDec.decodeArray(arrayname, array_size);
  if(retcode){
    Serial.print("Decoding returned ");
    Serial.println(retcode);
    renderJPEG(x, y);
  }else{
    Serial.println("Couldn't decode array");
  }
  Serial.println("#########################");
}


//####################################################################################################
// Setup
//####################################################################################################
void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  wifiMulti.addAP(WIFI_SSID, WIFI_PWD);
  
  Serial.println("Connecting Wifi...");
  if(wifiMulti.run(20000) == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
  }
  
  if (MDNS.begin("spotstation")) {
    Serial.println("MDNS responder started");
    MDNS.addService("_http","_tcp",80);
  }
  

  server.on("/", HTTP_GET, handleRoot);
  server.on("/upImg", HTTP_POST, [](AsyncWebServerRequest * request){}, NULL, handleUploadImage);
  server.on("/meta", HTTP_GET, handleMeta);
  server.onNotFound(handleNotFound);
  server.begin();
}
unsigned long initMill = 0;

//####################################################################################################
// Main loop
//####################################################################################################
void loop() {

}