/*
 * 卡路里计算器 - 基于摄像头拍照和GPT图像识别功能
 * 触屏拍照保存后，自动发送给GPT进行食物识别和卡路里计算，并在屏幕上显示结果
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

// WiFi和API配置
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
bool showing_result = false;       // 是否正在显示结果
String current_result = "";        // 当前显示的结果

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

// 在屏幕上显示卡路里计算结果
void displayCalorieResult(String result) {
    tft.fillScreen(TFT_BLACK);
    
    // 显示标题
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.println("CALORIE CALCULATOR");
    
    // 提取大括号中的总卡路里数值
    String calorieText = "";
    int braceStart = result.indexOf("{");
    int braceEnd = result.indexOf("}");
    
    if (braceStart != -1 && braceEnd != -1 && braceEnd > braceStart) {
        // 提取大括号中的内容
        String bracketContent = result.substring(braceStart + 1, braceEnd);
        bracketContent.trim();
        calorieText = bracketContent + " kcal";
        Serial.println("Extracted from braces: " + calorieText);
    } else {
        // 备用方法：如果没有大括号，使用最后一个kcal前的数字
        int lastKcalPos = result.lastIndexOf("kcal");
        if (lastKcalPos != -1) {
            int numEnd = lastKcalPos - 1;
            
            // 跳过空格
            while (numEnd >= 0 && result.charAt(numEnd) == ' ') {
                numEnd--;
            }
            
            // 找到数字的开始位置
            int numStart = numEnd;
            while (numStart >= 0 && (isDigit(result.charAt(numStart)) || result.charAt(numStart) == '.')) {
                numStart--;
            }
            numStart++; // 调整到数字开始位置
            
            if (numStart <= numEnd) {
                calorieText = result.substring(numStart, numEnd + 1) + " kcal";
                Serial.println("Extracted from last kcal: " + calorieText);
            }
        }
    }
    
    // 大字体显示卡路里
    if (calorieText.length() > 0) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(3);
        tft.setCursor(20, 50);
        tft.println(calorieText);
    }
    
    // 显示食物名称
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    int currentY = 100;
    
    // 简化显示其他信息
    int lineHeight = 12;
    int maxCharsPerLine = 28;
    int resultLen = result.length();
    
    for (int i = 0; i < resultLen && currentY < 200; i += maxCharsPerLine) {
        int endPos = min(i + maxCharsPerLine, resultLen);
        char line[32];
        int copyLen = min(endPos - i, 31);
        result.substring(i, i + copyLen).toCharArray(line, copyLen + 1);
        
        tft.setCursor(10, currentY);
        tft.println(line);
        currentY += lineHeight;
    }
    
    // 显示提示信息
    tft.setCursor(10, 220);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Touch for new photo");
    
    showing_result = true;
    current_result = "";
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

    Serial.printf("Sending image %s to GPT for calorie analysis...\n", filename);
    
    // 显示分析中的提示
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(50, 100);
    tft.println("Analyzing...");
    tft.setTextSize(1);
    tft.setCursor(60, 130);
    tft.println("Please wait");
    
    // 使用更简洁的提示词，总卡路里数值放在大括号中
    String response = chat.sendImageMessage(filename, 
        "Analyze this food image and calculate calories. "
        "Give me: Food name, estimated weight, calories per 100g, total calories. "
        "Format: [Food]: [weight]g, [cal/100g] kcal/100g = {[total]} kcal total. "
        "Keep response under 100 characters.");
    
    // 输出识别结果到串口
    Serial.println("=== GPT Calorie Analysis Result ===");
    Serial.println(response);
    Serial.printf("Response length: %d characters\n", response.length());
    Serial.println("==================================");
    
    // 检查响应是否完整
    if (response.length() < 10) {
        Serial.println("Warning: Response seems too short, might be truncated");
        response = "Error: Incomplete response from GPT. Please try again.";
    }
    
    // 在屏幕上显示结果
    displayCalorieResult(response);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Calorie Calculator Camera System...");

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
    
    // 显示欢迎界面
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(30, 80);
    tft.println("CALORIE");
    tft.setCursor(20, 110);
    tft.println("CALCULATOR");
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(40, 150);
    tft.println("Touch to start");
    
    Serial.println("Calorie Calculator ready! Touch screen to take photo and analyze calories.");
}

void loop() {
    if (sd_sign && camera_sign) {
        
        // 如果正在显示结果，等待触摸后重新开始拍照模式
        if (showing_result) {
            if (display_is_pressed()) {
                Serial.println("Touch detected, returning to camera mode");
                showing_result = false;
                current_result = "";
                
                // 显示准备拍照的提示
                tft.fillScreen(TFT_BLACK);
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.setTextSize(1);
                tft.setCursor(60, 110);
                tft.println("Ready to capture");
                tft.setCursor(70, 130);
                tft.println("Touch to photo");
                delay(1000);
            }
            delay(100);
            return;
        }

        // Take a photo
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Failed to get camera frame buffer");
            return;
        }
        
        if (display_is_pressed()) {
            Serial.println("Display is touched - taking photo for calorie analysis");
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
                if (!file) {
                    Serial.println("Failed to open file for writing");
                } else {
                    if (file.write(out_buf, out_len) == out_len) {
                        Serial.printf("Saved picture to Flash: %s\n", filename);
                        file.close();
                        
                        // 确保文件完全写入Flash，添加延时
                        delay(100);
                        
                        // 立即分析刚保存的图片进行卡路里计算
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
        } else {
            // 显示摄像头预览
            uint8_t* buf = fb->buf;
            uint32_t len = fb->len;
            tft.startWrite();
            tft.setAddrWindow(0, 0, camera_width, camera_height);
            tft.pushColors(buf, len);
            tft.endWrite();
        }
        
        // Release image buffer
        esp_camera_fb_return(fb);

        delay(10);
    }
}
