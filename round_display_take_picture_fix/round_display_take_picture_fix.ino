/*
 * This code was adapted from the original, developed by Seeed Studio: 
 * https://wiki.seeedstudio.com/xiao_esp32s3_camera_usage/#project-i-making-a-handheld-camera
 * 
 * Important changes:
 *  - It is not necessary to to cut off J3 on the XIAO ESP32S3 Sense expansion board. 
 *  It is possible to use the XIAO's SD Card Reader. For that you need use SD_CS_PIN as 21.
 * 
 * - The camera buffer data should be captured as RGB565 (raw image) with a 240x240 frame size  
 * to be displayed on the round display. This raw image should be converted to jpeg before save 
 * in the SD card. This can be done with the line: 
 * esp_err_t ret = frame2jpg(fb, 12, &out_buf, &out_len);
 * 
 * - The XCLK_FREQ_HZ should be reduced from 20KHz to 10KHz in order to prevent 
 * the message "no EV-VSYNC-OVF message" that appears on Serial Monitor (probably due the time 
 * added for JPEG conversion. 
 * 
 * Adapted by MRovai @02June23
 * 
 * 
*/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "esp_camera.h"
#include "SPIFFS.h"

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#define TOUCH_INT D7

/* 
 * NOTE: Since the XIAO EPS32S3 Sense is designed with three pull-up resistors 
 * R4~R6 connected to the SD card slot, and the round display also has 
 * pull-up resistors, the Round Display SD card cannot be read when both 
 * are used at the same time. To solve this problem, we need to cut off 
 * J3 on the XIAO ESP32S3 Sense expansion board.
 * https://wiki.seeedstudio.com/xiao_esp32s3_camera_usage/#preliminary-preparation
 */

//#define SD_CS_PIN 21 // ESP32S3 Sense SD Card Reader
// SD卡功能已禁用，改用Flash存储

#include "camera_pins.h"

// Width and height of round display
const int camera_width = 240;
const int camera_height = 240;

// File Counter
int imageCount = 1;
bool camera_sign = false;          // Check camera status
bool sd_sign = false;              // Check sd status

TFT_eSPI tft = TFT_eSPI();

// 打印内存使用情况
void printMemoryUsage() {
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
}

bool display_is_pressed(void)
{
    // 触摸时TOUCH_INT引脚会变为LOW
    if(digitalRead(TOUCH_INT) == LOW) {
        delay(50); // 增加延时以消除抖动
        if(digitalRead(TOUCH_INT) == LOW) {
            // 等待触摸释放，避免连续触发
            while(digitalRead(TOUCH_INT) == LOW) {
                delay(10);
            }
            delay(100); // 释放后的延时
            return true;
        }
    }
    return false;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
//  while(!Serial);

  // 初始化触摸引脚
  pinMode(TOUCH_INT, INPUT_PULLUP);
  Serial.println("Touch pin initialized");

  // Camera pinout
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; // Reduced XCLK_FREQ_HZ from 20KHz to 10KHz (no EV-VSYNC-OVF message)
  //config.frame_size = FRAMESIZE_UXGA;
  //config.frame_size = FRAMESIZE_SVGA;
  config.frame_size = FRAMESIZE_240X240;
  //config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.pixel_format = PIXFORMAT_RGB565;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.jpeg_quality = 0;
  config.fb_count = 1;
  
  // 检查PSRAM是否可用
  if(psramFound()){
    Serial.println("PSRAM found, using PSRAM for frame buffer");
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    Serial.println("PSRAM not found, using DRAM for frame buffer");
    config.fb_location = CAMERA_FB_IN_DRAM;
  }
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 0;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
    // 对于RGB565格式，减少帧缓冲区数量以节省内存
    if(psramFound()){
      config.fb_count = 1; // 即使有PSRAM也只用1个缓冲区
    } else {
      config.fb_count = 1; // DRAM情况下只用1个缓冲区
    }
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  Serial.println("Camera ready");
  camera_sign = true; // Camera initialization check passes

  // Display initialization
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);

  // 初始化SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  sd_sign = true; // 标记存储初始化完成
  
  // 打印初始内存使用情况
  printMemoryUsage();

}

void loop() {
  if( sd_sign && camera_sign){

    // Take a photo
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Failed to get camera frame buffer");
      return;
    }
    
    if(display_is_pressed()){
      Serial.println("display is touched");
      char filename[32];
      sprintf(filename, "/image%d.jpg", imageCount);
      
      // Save photo to file
      size_t out_len = 0;
      uint8_t* out_buf = NULL;
      esp_err_t ret = frame2jpg(fb, 254, &out_buf, &out_len);
      if (ret == false) {
        Serial.printf("JPEG conversion failed");
      } else {
      // 保存图片到Flash
      fs::File file = SPIFFS.open(filename, FILE_WRITE);
      if(!file){
        Serial.println("Failed to open file for writing");
      } else {
        if(file.write(out_buf, out_len) == out_len){
          Serial.printf("Saved picture to Flash: %s\n", filename);
          imageCount++;
        } else {
          Serial.println("Write failed");
        }
        file.close();
      }
      free(out_buf);
      
      // 打印内存使用情况
      printMemoryUsage();
      }
    }
  
    //  images
    uint8_t* buf = fb->buf;
    uint32_t len = fb->len;
    tft.startWrite();
    tft.setAddrWindow(0, 0, camera_width, camera_height);
    tft.pushColors(buf, len);
    tft.endWrite();
      
    // Release image buffer
    esp_camera_fb_return(fb);

    delay(10);
  }
}
