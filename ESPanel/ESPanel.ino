// Example for library:
// https://github.com/Bodmer/TJpg_Decoder

// This example renders a Jpeg file that is stored in an array within Flash (program) memory
// see panda.h tab.  The panda image file being ~13Kbytes.

#define USE_DMA
#define TOUCH_SDA  33
#define TOUCH_SCL  32
#define TOUCH_INT 21
#define TOUCH_RST 25

#include <WiFi.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <GT911.h>


uint16_t dmaBuffer1[16*16]; // Toggle buffer for 16*16 MCU block, 512bytes
uint16_t dmaBuffer2[16*16]; // Toggle buffer for 16*16 MCU block, 512bytes
uint8_t *img_buff;
uint16_t* dmaBufferPtr = dmaBuffer1;
bool dmaBufferSel = 0;
const char* ssid     = "bahus-iot";
const char* password = "OoRYNmIIF2KUsbfC";
const char* server = "http://192.168.1.200:8000/test";
uint32_t last_draw = 0;

struct GTPoint last_touch;

TFT_eSPI tft = TFT_eSPI();
HTTPClient http;
GT911 touch = GT911();

// This next function will be called during decoding of the jpeg file to render each
// 16x16 or 8x8 image tile (Minimum Coding Unit) to the TFT.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if ( y >= tft.height() ) return 0; // Stop further decoding as image is running off bottom of screen
  // Double buffering is used, the bitmap is copied to the buffer by pushImageDMA() the
  // bitmap can then be updated by the jpeg decoder while DMA is in progress
  if (dmaBufferSel) dmaBufferPtr = dmaBuffer2;
  else dmaBufferPtr = dmaBuffer1;
  dmaBufferSel = !dmaBufferSel; // Toggle buffer selection
  tft.pushImageDMA(x, y, w, h, bitmap, dmaBufferPtr); // Initiate DMA - blocking only if last DMA is not complete
  // The DMA transfer of image block to the TFT is now in progress...
  // Return 1 to decode next block.
  return 1;
}

void setup(){
  Serial.begin(115200);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  img_buff = (uint8_t*)malloc(100*1024);

  Wire.setPins(TOUCH_SDA, TOUCH_SCL);
  touch.begin();

  // Initialise the TFT
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.initDMA(); // To use SPI DMA you must call initDMA() to setup the DMA engine

  TJpgDec.setJpgScale(1);

  // The colour byte order can be swapped by the decoder
  // using TJpgDec.setSwapBytes(true); or by the TFT_eSPI library:
  tft.setSwapBytes(true);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);
}

void draw() {
  Serial.println("http request");
  uint32_t start = millis();
  http.begin(server);
  int http_code = http.GET();

  if (http_code != HTTP_CODE_OK) return;
  size_t len = http.getSize();
  if (len < 0) {
    Serial.println("Unknown length");
    return;
  }
  size_t off = 0;

  Serial.println("streaming");
  WiFiClient *stream = http.getStreamPtr();

  while(http.connected() && off < len) {
      size_t available = stream->available();
      if(available) {
          int c = stream->readBytes(img_buff+off, available);
          off += c;
      }
  }
  
  Serial.print("HTTP: "); Serial.print(millis() - start); Serial.println("ms");
  start = millis();
  
  tft.startWrite();
  TJpgDec.drawJpg(0, 0, img_buff, len);
  tft.endWrite();
  Serial.print("JPG: "); Serial.print(millis() - start); Serial.println("ms");
}

void handle_touch() {
  uint8_t touches = touch.touched(GT911_MODE_POLLING);
  bool found = false;
  if (touches) {
    GTPoint* tp = touch.getPoints();
    for (uint8_t  i = 0; i < touches; i++) {
      if (tp[i].trackId == last_touch.trackId || last_touch.trackId == 255) {
        last_touch = tp[i];
        found = true;
      }
    }
  }
  if (!found && last_touch.trackId != 255) {
    last_touch.trackId = 255;
    send_touch();
    Serial.printf("Touch released %d,%d s:%d\n", last_touch.x, last_touch.y, last_touch.area);
  }
}

void send_touch() {
  http.begin(server);
  String request_data = "x="+String(last_touch.x)+"&y="+String(last_touch.y);
  int http_code = http.POST(request_data);

}

void loop(){
  handle_touch();
  if (millis() - last_draw > 100) {
    draw();
    last_draw = millis();
  }
}
