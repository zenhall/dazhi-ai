#ifndef ArduinoGPTChat_h
#define ArduinoGPTChat_h

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Audio.h"

class ArduinoGPTChat {
  public:
    ArduinoGPTChat(const char* apiKey);
    String sendMessage(String message);
    bool textToSpeech(String text);
    
  private:
    const char* _apiKey;
    String _apiUrl = "https://api.chatanywhere.tech/v1/chat/completions";
    String _ttsApiUrl = "https://api.chatanywhere.tech/v1/audio/speech";
    String _buildPayload(String message);
    String _processResponse(String response);
    String _buildTTSPayload(String text);
};

#endif
