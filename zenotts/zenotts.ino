#include <WiFi.h>  
#include <ArduinoGPTChat.h>
#include "Audio.h"

// 定义I2S引脚
#define I2S_DOUT 3
#define I2S_BCLK 2
#define I2S_LRC 1

// Replace with your network credentials  
const char* ssid     = "2nd-curv";  
const char* password = "xbotpark";  

// Replace with your OpenAI API key  
const char* apiKey = "sk-CkxIb6MfdTBgZkdm0MtUEGVGk6Q6o5X5BRB1DwE2BdeSLSqB";  

// 全局声明audio变量，让ArduinoGPTChat可以访问
Audio audio;

// Initialize ArduinoGPTChat instance
ArduinoGPTChat gptChat(apiKey);

bool gettingResponse = true;  
  
void setup()   
{  
  // Initialize Serial  
  Serial.begin(9600);  
  delay(500); // Give serial port some time to initialize  
  
  // Connect to Wi-Fi network  
  WiFi.mode(WIFI_STA);  
  WiFi.begin(ssid, password);  
  Serial.println("Connecting to WiFi ..");  
  
  int attempt = 0;  
  while (WiFi.status() != WL_CONNECTED && attempt < 20)   
  {  
      Serial.print('.');  
      delay(1000);  
      attempt++;  
  }  
  
  if (WiFi.status() == WL_CONNECTED) {  
    Serial.println("\nConnected to WiFi!");  
    Serial.print("IP Address: ");  
    Serial.println(WiFi.localIP());  
    Serial.println("Ready for commands. Type your message or 'TTS:your text' for speech conversion.");  
    
    // 设置I2S引脚
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    // 设置音量
    audio.setVolume(100);
  } else {  
    Serial.println("\nFailed to connect to WiFi. Please check credentials and try again.");  
  }  
}  

void loop()   
{  
    // 处理音频循环
    audio.loop();
    
    if(Serial.available() > 0)  
    {  
      String prompt = Serial.readStringUntil('\n');  
      prompt.trim();  
      if (prompt.length() > 0) {  
        Serial.print("USER: ");  
        Serial.println(prompt);  
        gettingResponse = true;  
        
        // Check if it's a TTS command  
        if (prompt.startsWith("TTS:")) {  
          // Extract the text to convert to speech  
          String ttsText = prompt.substring(4);  
          ttsText.trim();  
          if (ttsText.length() > 0) {  
            ttsCall(ttsText);  
          } else {  
            Serial.println("Empty TTS text. Please use format 'TTS:your text'");  
          }  
        } else {  
          chatGptCall(prompt);  
        }  
      }  
    }  
    delay(10); // Small delay to prevent CPU overload  
}   

void chatGptCall(String message)  
{  
  Serial.println("Sending request to ChatGPT...");  
  
  String response = gptChat.sendMessage(message);
  
  if (response != "") {  
    Serial.print("CHATGPT: ");  
    Serial.println(response);  
    
    // Automatically convert the response to speech if not empty  
    if (response.length() > 0) {  
      ttsCall(response);  
    }  
    gettingResponse = false;  
  } else {  
    Serial.println("Failed to get response from ChatGPT");  
    gettingResponse = false;  
  }  
}  

void ttsCall(String text) {
  Serial.println("Converting text to speech...");
  
  // 使用修改后的textToSpeech方法，它会内部调用audio.openai_speech
  bool success = gptChat.textToSpeech(text);
  
  if (success) {
    Serial.println("TTS audio is playing through speaker");
  } else {
    Serial.println("Failed to play TTS audio");
  }
}
