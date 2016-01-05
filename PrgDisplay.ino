#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>
#include <TouchScreen.h>

#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin

#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940

#define LCD_CS A3
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
#define LCD_RESET A4
#define SD_CS 10     // Set the chip select line to whatever you use (10 doesnt conflict with the library)

#define	BLACK   0x0000
#define	RED     0xF800
#define	BLUE    0x001F
#define	GREEN   0x07E0
//#define CYAN    0x07FF
//#define MAGENTA 0xF81F
//#define YELLOW  0xFFE0
//#define WHITE   0xFFFF
#define MINPRESSURE 10
#define MAXPRESSURE 1000
#define BOXSIZE 40

#define BUFFPIXEL 20

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

void setup(void) {
  Serial.begin(9600);
  
  tft.reset();  
  uint16_t identifier = tft.readID();
  tft.begin(identifier);
  tft.setTextSize(2);
  tft.fillScreen(0);
  tft.setRotation(3);
  if (!SD.begin(SD_CS)) {
    return;
  }
  initMenu();
}

byte val_new = 0;
byte val_old = 0;

void loop()
{
  checkMenu();
  if(val_old != val_new) {
    switch(val_new) { 
     case 1:
       bmpDraw(SD.open("Rach1.bmp"));
       break;      
     case 2:
       bmpDraw(SD.open("Rach2.bmp"));
       break;
     case 3:
       tft.fillRect(0, BOXSIZE, tft.width(), tft.height()-BOXSIZE, BLACK);
       break;
    }
  }
  val_old = val_new;  
}

//void drawPictures() {
//  File pictures = SD.open("/Pictures/");
//  while(true) {
//     File entry =  pictures.openNextFile();
//     if (!entry) {
//       break;
//     }
//     bmpDraw(entry);
//     delay(2000);
//   }
//}

//void bmpDraw(char* bmpFileName) {
////  if(SD.exists(bmpFileName)) {
//    bmpDraw(SD.open(bmpFileName));
////  } 
//}

void bmpDraw(File bmpFile) {
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel in buffer (R+G+B per pixel)
  uint16_t lcdbuffer[BUFFPIXEL];  // pixel out buffer (16-bit per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  uint16_t row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0;
  uint8_t  lcdidx = 0;
  boolean  first = true;
  tft.setRotation(3);  
  
  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) {
    read32(bmpFile); // Size of file
    read32(bmpFile); // Creator Bytes    
    bmpImageoffset = read32(bmpFile); // Start of image data       
    read32(bmpFile); // Size of BIP Header    
    bmpWidth  = read32(bmpFile); // BMP Width
    bmpHeight = read32(bmpFile); // BMP Height
    
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      if((read16(bmpFile) == 24) && (read32(bmpFile) == 0)) { 
        goodBmp = true;         
        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;       
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }
        
        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(0, 0, bmpWidth-1, bmpHeight-1);
        
        for (row=0; row<bmpHeight; row++) { 
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<bmpWidth; col++) { // For each column...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              // Push LCD buffer to the display first
              if(lcdidx > 0) {
                tft.pushColors(lcdbuffer, lcdidx, first);
                lcdidx = 0;
                first  = false;
              }
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = tft.color565(r,g,b);
          } // end pixel
        } // end scanline
        // Write any remaining data to LCD
        if(lcdidx > 0) {
          tft.pushColors(lcdbuffer, lcdidx, first);
        }
      } // end goodBmp
    }
  }
  bmpFile.close();
}

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void initMenu() {
  tft.setRotation(0);  
  tft.fillScreen(0);
  tft.fillRect(0, 0, BOXSIZE, BOXSIZE, RED);
  tft.fillRect(BOXSIZE, 0, BOXSIZE, BOXSIZE, BLUE);
  tft.fillRect(BOXSIZE*2, 0, BOXSIZE, BOXSIZE, GREEN);
  
  pinMode(13, OUTPUT);
}

void checkMenu() {
  tft.setRotation(0);
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  digitalWrite(13, LOW);
  
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
    
    p.x = tft.width() - map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
    p.y = map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);
    
    if (p.y < BOXSIZE) {
       if (p.x < BOXSIZE) { 
         val_new = 1;
       } else if (p.x < BOXSIZE*2) {
         val_new = 2;
       } else if (p.x < BOXSIZE*3) {
         val_new = 3; 
       }
    }
  }
}
