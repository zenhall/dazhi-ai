#ifndef ArduinoGPTChat_h
#define ArduinoGPTChat_h

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Audio.h"
#include "FS.h"
#include "SD.h"

class ArduinoGPTChat {
  public:
    ArduinoGPTChat(const char* apiKey);
    String sendMessage(String message);
    bool textToSpeech(String text);
    String speechToText(const char* audioFilePath);
    String speechToTextFromBuffer(uint8_t* audioBuffer, size_t bufferSize);
    String sendImageMessage(const char* imageFilePath, String question);
    
  private:
    void base64_encode(const uint8_t* input, size_t length, char* output);
    size_t base64_encode_length(size_t input_length);
    const char* _apiKey;
    String _apiUrl = "https://api.chatanywhere.tech/v1/chat/completions";
    String _ttsApiUrl = "https://api.chatanywhere.tech/v1/audio/speech";
    String _sttApiUrl = "https://api.chatanywhere.tech/v1/audio/transcriptions";
    String _buildPayload(String message);
    String _processResponse(String response);
    String _buildTTSPayload(String text);
    String _buildMultipartForm(const char* audioFilePath, String boundary);
};

#endif
