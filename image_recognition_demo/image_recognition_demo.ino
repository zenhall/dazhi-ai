#include <WiFi.h>
#include "ArduinoGPTChat.h"
#include <SPIFFS.h>

// 替换为你的Wi-Fi和API密钥信息
const char* ssid = "2nd-curv";
const char* password = "xbotpark";
const char* apiKey = "sk-CkxIb6MfdTBgZkdm0MtUEGVGk6Q6o5X5BRB1DwE2BdeSLSqB";

ArduinoGPTChat chat(apiKey);

void setup() {
  Serial.begin(115200);
  while(!Serial) {
    ; // 等待串口连接
  }

  // 连接WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // 初始化SPIFFS
  if(!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  
  // 检查test1.png是否存在
  if(!SPIFFS.exists("/image1.jpg")) {
    Serial.println("Cannot find test1.png in flash");
    return;
  }

  Serial.println("Sending image to GPT for analysis...");
  
  // 发送图片和问题给GPT
  String response = chat.sendImageMessage("/image1.jpg", "图片里的人在干什么？");
  
  // 输出识别结果
  Serial.println("GPT Response:");
  Serial.println(response);
}

void loop() {
  // 空循环
}
