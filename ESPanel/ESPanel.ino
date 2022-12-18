// Example for library:
// https://github.com/Bodmer/TJpg_Decoder

// This example renders a Jpeg file that is stored in an array within Flash (program) memory
// see panda.h tab.  The panda image file being ~13Kbytes.

#define USE_DMA

// Include the array
#include <WiFi.h>
#include <HTTPClient.h>

// Include the jpeg decoder library
#include <TJpg_Decoder.h>

uint16_t dmaBuffer1[16*16]; // Toggle buffer for 16*16 MCU block, 512bytes
uint16_t dmaBuffer2[16*16]; // Toggle buffer for 16*16 MCU block, 512bytes
uint8_t *img_buff;
uint16_t* dmaBufferPtr = dmaBuffer1;
bool dmaBufferSel = 0;

// Include the TFT library https://github.com/Bodmer/TFT_eSPI
#include "SPI.h"
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

HTTPClient http;

const char* ssid     = "bahus-iot";
const char* password = "OoRYNmIIF2KUsbfC";

// This next function will be called during decoding of the jpeg file to render each
// 16x16 or 8x8 image tile (Minimum Coding Unit) to the TFT.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
   // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;
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

  // Initialise the TFT
  tft.begin();
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);

  tft.initDMA(); // To use SPI DMA you must call initDMA() to setup the DMA engine

  TJpgDec.setJpgScale(1);

  // The colour byte order can be swapped by the decoder
  // using TJpgDec.setSwapBytes(true); or by the TFT_eSPI library:
  tft.setSwapBytes(true);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);
}

void loop(){
  uint32_t start = millis();

  Serial.println("http request");
  http.begin("http://192.168.1.18:8000/image.jpg");
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

  delay(2000);
}
