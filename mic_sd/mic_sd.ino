#include "ESP_I2S.h"
#include "FS.h"
#include "SD.h"
#include <WiFi.h>
#include "ArduinoGPTChat.h"

// WiFi设置
const char* ssid = "2nd-curv";
const char* password = "xbotpark";

// API密钥
const char* apiKey = "sk-CkxIb6MfdTBgZkdm0MtUEGVGk6Q6o5X5BRB1DwE2BdeSLSqB"; // 替换为您的API密钥

// 音频文件路径
const char* audioFilePath = "/arduinor_rec.wav";

// 创建GPT对话实例
ArduinoGPTChat gptChat(apiKey);

void setup() {
  // Create an instance of the I2SClass
  I2SClass i2s;

  // Create variables to store the audio data
  uint8_t *wav_buffer;
  size_t wav_size;

  // Initialize the serial port
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("Initializing I2S bus...");

  // Set up the pins used for audio input
  i2s.setPinsPdmRx(42, 41);

  // start I2S at 16 kHz with 16-bits per sample
  if (!i2s.begin(I2S_MODE_PDM_RX, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("Failed to initialize I2S!");
    while (1); // do nothing
  }

  Serial.println("I2S bus initialized.");
  Serial.println("Initializing SD card...");

  // Set up the pins used for SD card access
  if(!SD.begin(21)){
    Serial.println("Failed to mount SD Card!");
    while (1) ;
  }
  Serial.println("SD card initialized.");
  Serial.println("Recording 20 seconds of audio data...");

  // Record 3 seconds of audio data (可以根据需要调整录音时间)
  Serial.println("Recording audio for 3 seconds...");
  wav_buffer = i2s.recordWAV(3, &wav_size);
  
  if (wav_buffer == NULL || wav_size == 0) {
    Serial.println("Failed to record audio or buffer is empty!");
    return;
  }
  
  Serial.println("Audio recorded successfully, size: " + String(wav_size) + " bytes");

  // Create a file on the SD card
  File file = SD.open("/arduinor_rec.wav", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing!");
    return;
  }

  Serial.println("Writing audio data to file...");

  // Write the audio data to the file
  size_t bytesWritten = file.write(wav_buffer, wav_size);
  if (bytesWritten != wav_size) {
    Serial.println("Failed to write audio data to file!");
    Serial.println("Wrote " + String(bytesWritten) + " of " + String(wav_size) + " bytes");
    file.close();
    free(wav_buffer); // 释放缓冲区内存
    return;
  }

  // Close the file
  file.close();
  
  // 释放缓冲区内存
  free(wav_buffer);

  Serial.println("Audio recording saved to SD card.");
  
  // 连接WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  // 调用STT功能将音频转换为文本
  Serial.println("Converting speech to text...");
  Serial.println("Using audio file: " + String(audioFilePath));
  Serial.println("This may take some time depending on the file size...");
  
  String transcribedText = gptChat.speechToText(audioFilePath);
  
  // 显示转换结果
  Serial.println("Transcription result:");
  if (transcribedText.length() > 0) {
    Serial.println(transcribedText);
  } else {
    Serial.println("No text was transcribed or an error occurred.");
    Serial.println("Check serial output above for error messages.");
  }
  
  Serial.println("Application complete.");
}

void loop() {
  delay(1000);
  Serial.printf(".");
}
