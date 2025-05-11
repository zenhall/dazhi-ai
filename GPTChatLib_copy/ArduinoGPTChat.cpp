#include "ArduinoGPTChat.h"

ArduinoGPTChat::ArduinoGPTChat(const char* apiKey) {
  _apiKey = apiKey;
}

String ArduinoGPTChat::sendMessage(String message) {
  HTTPClient http;
  http.begin(_apiUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(_apiKey));

  String payload = _buildPayload(message);
  int httpResponseCode = http.POST(payload);

  if (httpResponseCode == 200) {
    String response = http.getString();
    return _processResponse(response);
  }
  return "";
}

String ArduinoGPTChat::_buildPayload(String message) {
  DynamicJsonDocument doc(512);
  doc["model"] = "gpt-4.1-nano";
  JsonArray messages = doc.createNestedArray("messages");
  JsonObject msg = messages.createNestedObject();
  msg["role"] = "user";
  msg["content"] = message;

  String output;
  serializeJson(doc, output);
  return output;
}

String ArduinoGPTChat::_processResponse(String response) {
  DynamicJsonDocument jsonDoc(1024);
  deserializeJson(jsonDoc, response);
  String outputText = jsonDoc["choices"][0]["message"]["content"];
  outputText.remove(outputText.indexOf('\n'));
  return outputText;
}

bool ArduinoGPTChat::textToSpeech(String text) {
  // 创建一个临时的Audio对象
  extern Audio audio;
  
  // 使用Audio库的openai_speech功能
  return audio.openai_speech(
    String(_apiKey),     // API密钥
    "gpt-4o-mini-tts",   // 模型
    text,                // 输入文本
    "alloy",             // 声音
    "mp3",               // 响应格式
    "1.0"                // 语速
  );
}

String ArduinoGPTChat::_buildTTSPayload(String text) {
  text.replace("\"", "\\\"");
  return "{\"model\": \"gpt-4o-mini-tts\", \"input\": \"" + text + "\", \"voice\": \"alloy\"}";
}
