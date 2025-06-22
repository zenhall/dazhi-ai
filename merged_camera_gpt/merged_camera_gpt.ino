/*
 * 融合摄像头拍照和GPT图像识别功能
 * 触屏拍照保存后，自动发送给GPT进行分析，并通过串口打印结果
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "esp_camera.h"
#include "SPIFFS.h"
#include <WiFi.h>
#include "ArduinoGPTChat.h"

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#define TOUCH_INT D7

// // WiFi和API配置
const char* ssid = "2nd-curv";
const char* password = "xbotpark";
// const char* ssid = "zh";
// const char* password = "6666666";
const char* apiKey = "sk-CkxIb6MfdTBgZkdm0MtUEGVGk6Q6o5X5BRB1DwE2BdeSLSqB";

// 摄像头引脚定义
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// Width and height of round display
const int camera_width = 240;
const int camera_height = 240;

// File Counter
int imageCount = 1;
bool camera_sign = false;          // Check camera status
bool sd_sign = false;              // Check storage status
bool wifi_connected = false;       // WiFi connection status

TFT_eSPI tft = TFT_eSPI();
ArduinoGPTChat chat(apiKey);

// 扫描SPIFFS中已有的图片文件，找到最大编号
void findMaxImageNumber() {
    int maxNumber = 0;
    
    // 打开根目录
    File root = SPIFFS.open("/");
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }
    
    // 遍历目录中的所有文件
    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        Serial.printf("Found file: %s\n", fileName.c_str());
        
        // 检查文件名是否符合 "imageXX.jpg" 格式（支持有无前导斜杠的情况）
        bool isImageFile = false;
        String numberStr = "";
        
        if (fileName.startsWith("/image") && fileName.endsWith(".jpg")) {
            // 有前导斜杠的情况：/imageXX.jpg
            numberStr = fileName.substring(6); // 去掉 "/image" 前缀
            numberStr = numberStr.substring(0, numberStr.length() - 4); // 去掉 ".jpg" 后缀
            isImageFile = true;
        } else if (fileName.startsWith("image") && fileName.endsWith(".jpg")) {
            // 无前导斜杠的情况：imageXX.jpg
            numberStr = fileName.substring(5); // 去掉 "image" 前缀
            numberStr = numberStr.substring(0, numberStr.length() - 4); // 去掉 ".jpg" 后缀
            isImageFile = true;
        }
        
        if (isImageFile && numberStr.length() > 0) {
            int number = numberStr.toInt();
            if (number > 0 && number > maxNumber) { // 确保是有效的正数
                maxNumber = number;
            }
            Serial.printf("Extracted number: %d, current max: %d\n", number, maxNumber);
        }
        
        file = root.openNextFile();
    }
    
    // 设置下一个文件编号
    imageCount = maxNumber + 1;
    Serial.printf("Next image will be saved as: image%d.jpg\n", imageCount);
}

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

void connectWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected successfully!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        wifi_connected = true;
    } else {
        Serial.println("\nWiFi connection failed!");
        wifi_connected = false;
    }
}

void analyzeImageWithGPT(const char* filename) {
    if (!wifi_connected) {
        Serial.println("WiFi not connected, cannot analyze image");
        return;
    }
    
    // 检查文件是否存在
    if(!SPIFFS.exists(filename)) {
        Serial.printf("Cannot find %s in flash\n", filename);
        return;
    }

    Serial.printf("Sending image %s to GPT for analysis...\n", filename);
    
    // 发送图片和问题给GPT
    String response = chat.sendImageMessage(filename, "请识别一下这是什么种类的植物或者动物。");
    
    // 输出识别结果
    Serial.println("=== GPT Analysis Result ===");
    Serial.println(response);
    Serial.println("=========================");
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting merged camera and GPT system...");

    // 初始化触摸引脚
    pinMode(TOUCH_INT, INPUT_PULLUP);
    Serial.println("Touch pin initialized");

    // 连接WiFi
    connectWiFi();

    // 摄像头配置
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
    config.xclk_freq_hz = 10000000; // Reduced XCLK_FREQ_HZ from 20KHz to 10KHz
    config.frame_size = FRAMESIZE_240X240;
    config.pixel_format = PIXFORMAT_RGB565;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    
    // 检查PSRAM是否可用
    if(psramFound()){
        Serial.println("PSRAM found, using PSRAM for frame buffer");
        config.fb_location = CAMERA_FB_IN_PSRAM;
    } else {
        Serial.println("PSRAM not found, using DRAM for frame buffer");
        config.fb_location = CAMERA_FB_IN_DRAM;
    }
    
    // 摄像头初始化
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return;
    }
    Serial.println("Camera ready");
    camera_sign = true;

    // 显示屏初始化
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_WHITE);

    // 初始化SPIFFS
    if(!SPIFFS.begin(true)){
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    sd_sign = true;
    Serial.println("SPIFFS initialized");
    
    // 扫描已有文件，设置正确的文件编号
    findMaxImageNumber();
    
    // 打印初始内存使用情况
    printMemoryUsage();
    
    Serial.println("System ready! Touch screen to take photo and analyze with GPT.");
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
      esp_err_t ret = frame2jpg(fb, 255, &out_buf, &out_len);
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
          file.close();
          
          // 确保文件完全写入Flash，添加延时
          delay(100);
          
          // 立即分析刚保存的图片
          analyzeImageWithGPT(filename);
          
          imageCount++;
        } else {
          Serial.println("Write failed");
          file.close();
        }
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
