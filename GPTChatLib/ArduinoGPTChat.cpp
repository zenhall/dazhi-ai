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
  DynamicJsonDocument doc(768);
  doc["model"] = "gpt-4.1-nano";
  JsonArray messages = doc.createNestedArray("messages");
  
  // 添加系统消息，要求简短回答
  JsonObject sysMsg = messages.createNestedObject();
  sysMsg["role"] = "system";
  sysMsg["content"] = "请用简短的语言回答问题，回答不要超过30个字。避免过长的解释，直接给出关键信息。";
  
  // 添加用户消息
  JsonObject userMsg = messages.createNestedObject();
  userMsg["role"] = "user";
  userMsg["content"] = message;

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

String ArduinoGPTChat::speechToText(const char* audioFilePath) {
  String response = "";
  
  // 检查文件是否存在
  if (!SD.exists(audioFilePath)) {
    Serial.println("Audio file not found: " + String(audioFilePath));
    return response;
  }
  
  // 打开音频文件
  File audioFile = SD.open(audioFilePath, FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open audio file!");
    return response;
  }
  
  // 获取文件大小
  size_t fileSize = audioFile.size();
  Serial.println("Audio file size: " + String(fileSize) + " bytes");
  
  // 创建一个临时缓冲区来存储整个文件内容
  // 注意：这可能会占用大量内存，如果文件很大，可能会失败
  uint8_t* fileData = (uint8_t*)malloc(fileSize);
  if (!fileData) {
    Serial.println("Failed to allocate memory for file!");
    audioFile.close();
    return response;
  }
  
  // 读取文件内容到缓冲区
  size_t bytesRead = audioFile.read(fileData, fileSize);
  audioFile.close();
  
  if (bytesRead != fileSize) {
    Serial.println("Failed to read entire file!");
    free(fileData);
    return response;
  }
  
  Serial.println("File read into memory successfully.");
  
  // 使用与Python示例相同的边界
  String boundary = "wL36Yn8afVp8Ag7AmP8qZ0SA4n1v9T";
  
  // 构建multipart/form-data请求体的各个部分
  // 文件部分
  String part1 = "--" + boundary + "\r\n";
  part1 += "Content-Disposition: form-data; name=file; filename=audio.wav\r\n";
  part1 += "Content-Type: audio/wav\r\n\r\n";
  
  // 模型部分
  String part2 = "\r\n--" + boundary + "\r\n";
  part2 += "Content-Disposition: form-data; name=model;\r\n";
  part2 += "Content-Type: text/plain\r\n\r\n";
  part2 += "whisper-1";
  
  // 提示部分（与Python示例匹配）
  String part3 = "\r\n--" + boundary + "\r\n";
  part3 += "Content-Disposition: form-data; name=prompt;\r\n";
  part3 += "Content-Type: text/plain\r\n\r\n";
  part3 += "eiusmod nulla";
  
  // 响应格式部分
  String part4 = "\r\n--" + boundary + "\r\n";
  part4 += "Content-Disposition: form-data; name=response_format;\r\n";
  part4 += "Content-Type: text/plain\r\n\r\n";
  part4 += "json";
  
  // 温度部分
  String part5 = "\r\n--" + boundary + "\r\n";
  part5 += "Content-Disposition: form-data; name=temperature;\r\n";
  part5 += "Content-Type: text/plain\r\n\r\n";
  part5 += "0";
  
  // 语言部分（与Python示例匹配）
  String part6 = "\r\n--" + boundary + "\r\n";
  part6 += "Content-Disposition: form-data; name=language;\r\n";
  part6 += "Content-Type: text/plain\r\n\r\n";
  part6 += "";
  
  // 结束边界
  String part7 = "\r\n--" + boundary + "--\r\n";
  
  // 计算总内容长度
  size_t totalLength = part1.length() + fileSize + part2.length() + part3.length() + 
                      part4.length() + part5.length() + part6.length() + part7.length();
  
  // 初始化HTTP客户端
  HTTPClient http;
  http.begin(_sttApiUrl);
  
  // 设置请求头
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Authorization", "Bearer " + String(_apiKey));
  http.addHeader("Content-Length", String(totalLength));
  
  // 合并所有部分为一个完整的请求体
  Serial.println("Preparing request body...");
  uint8_t* requestBody = (uint8_t*)malloc(totalLength);
  if (!requestBody) {
    Serial.println("Failed to allocate memory for request body!");
    free(fileData);
    return response;
  }
  
  // 拷贝各部分到请求体
  size_t pos = 0;
  
  // 拷贝 part1
  memcpy(requestBody + pos, part1.c_str(), part1.length());
  pos += part1.length();
  
  // 拷贝文件数据
  memcpy(requestBody + pos, fileData, fileSize);
  pos += fileSize;
  free(fileData); // 释放文件数据的内存
  
  // 拷贝其余各部分
  memcpy(requestBody + pos, part2.c_str(), part2.length());
  pos += part2.length();
  
  memcpy(requestBody + pos, part3.c_str(), part3.length());
  pos += part3.length();
  
  memcpy(requestBody + pos, part4.c_str(), part4.length());
  pos += part4.length();
  
  memcpy(requestBody + pos, part5.c_str(), part5.length());
  pos += part5.length();
  
  memcpy(requestBody + pos, part6.c_str(), part6.length());
  pos += part6.length();
  
  memcpy(requestBody + pos, part7.c_str(), part7.length());
  pos += part7.length();
  
  // 确认总长度匹配
  if (pos != totalLength) {
    Serial.println("Warning: actual body length doesn't match calculated length");
    Serial.println("Calculated: " + String(totalLength) + ", Actual: " + String(pos));
  }
  
  // 发送请求
  Serial.println("Sending STT request...");
  int httpCode = http.POST(requestBody, totalLength);
  
  // 释放请求体内存
  free(requestBody);
  
  Serial.print("HTTP Response Code: ");
  Serial.println(httpCode);
  
  if (httpCode == 200) {
    // 获取响应体
    response = http.getString();
    Serial.println("Got STT response: " + response);
    
    // 解析JSON响应
    DynamicJsonDocument jsonDoc(1024);
    DeserializationError error = deserializeJson(jsonDoc, response);
    
    if (!error) {
      // 提取转录的文本
      response = jsonDoc["text"].as<String>();
    } else {
      Serial.print("JSON parsing error: ");
      Serial.println(error.c_str());
      response = "";
    }
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(httpCode);
    // 尝试获取错误响应内容
    String errorResponse = http.getString();
    if (errorResponse.length() > 0) {
      Serial.println("Error response: " + errorResponse);
    }
    response = "";
  }
  
  http.end();
  return response;
}

String ArduinoGPTChat::_buildMultipartForm(const char* audioFilePath, String boundary) {
  // 此方法不再使用，保留方法签名以保持兼容性
  return "";
}

String ArduinoGPTChat::speechToTextFromBuffer(uint8_t* audioBuffer, size_t bufferSize) {
  String response = "";
  
  if (audioBuffer == NULL || bufferSize == 0) {
    Serial.println("Invalid audio buffer or size!");
    return response;
  }
  
  Serial.println("Audio buffer size: " + String(bufferSize) + " bytes");
  
  // 使用与Python示例相同的边界
  String boundary = "wL36Yn8afVp8Ag7AmP8qZ0SA4n1v9T";
  
  // 构建multipart/form-data请求体的各个部分
  // 文件部分
  String part1 = "--" + boundary + "\r\n";
  part1 += "Content-Disposition: form-data; name=file; filename=audio.wav\r\n";
  part1 += "Content-Type: audio/wav\r\n\r\n";
  
  // 模型部分
  String part2 = "\r\n--" + boundary + "\r\n";
  part2 += "Content-Disposition: form-data; name=model;\r\n";
  part2 += "Content-Type: text/plain\r\n\r\n";
  part2 += "whisper-1";
  
  // 提示部分（与Python示例匹配）
  String part3 = "\r\n--" + boundary + "\r\n";
  part3 += "Content-Disposition: form-data; name=prompt;\r\n";
  part3 += "Content-Type: text/plain\r\n\r\n";
  part3 += "eiusmod nulla";
  
  // 响应格式部分
  String part4 = "\r\n--" + boundary + "\r\n";
  part4 += "Content-Disposition: form-data; name=response_format;\r\n";
  part4 += "Content-Type: text/plain\r\n\r\n";
  part4 += "json";
  
  // 温度部分
  String part5 = "\r\n--" + boundary + "\r\n";
  part5 += "Content-Disposition: form-data; name=temperature;\r\n";
  part5 += "Content-Type: text/plain\r\n\r\n";
  part5 += "0";
  
  // 语言部分（与Python示例匹配）
  String part6 = "\r\n--" + boundary + "\r\n";
  part6 += "Content-Disposition: form-data; name=language;\r\n";
  part6 += "Content-Type: text/plain\r\n\r\n";
  part6 += "";
  
  // 结束边界
  String part7 = "\r\n--" + boundary + "--\r\n";
  
  // 计算总内容长度
  size_t totalLength = part1.length() + bufferSize + part2.length() + part3.length() + 
                      part4.length() + part5.length() + part6.length() + part7.length();
  
  // 初始化HTTP客户端
  HTTPClient http;
  http.begin(_sttApiUrl);
  
  // 设置请求头
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Authorization", "Bearer " + String(_apiKey));
  http.addHeader("Content-Length", String(totalLength));
  
  // 合并所有部分为一个完整的请求体
  Serial.println("Preparing request body...");
  uint8_t* requestBody = (uint8_t*)malloc(totalLength);
  if (!requestBody) {
    Serial.println("Failed to allocate memory for request body!");
    return response;
  }
  
  // 拷贝各部分到请求体
  size_t pos = 0;
  
  // 拷贝 part1
  memcpy(requestBody + pos, part1.c_str(), part1.length());
  pos += part1.length();
  
  // 拷贝音频数据
  memcpy(requestBody + pos, audioBuffer, bufferSize);
  pos += bufferSize;
  
  // 拷贝其余各部分
  memcpy(requestBody + pos, part2.c_str(), part2.length());
  pos += part2.length();
  
  memcpy(requestBody + pos, part3.c_str(), part3.length());
  pos += part3.length();
  
  memcpy(requestBody + pos, part4.c_str(), part4.length());
  pos += part4.length();
  
  memcpy(requestBody + pos, part5.c_str(), part5.length());
  pos += part5.length();
  
  memcpy(requestBody + pos, part6.c_str(), part6.length());
  pos += part6.length();
  
  memcpy(requestBody + pos, part7.c_str(), part7.length());
  pos += part7.length();
  
  // 确认总长度匹配
  if (pos != totalLength) {
    Serial.println("Warning: actual body length doesn't match calculated length");
    Serial.println("Calculated: " + String(totalLength) + ", Actual: " + String(pos));
  }
  
  // 发送请求
  Serial.println("Sending STT request...");
  int httpCode = http.POST(requestBody, totalLength);
  
  // 释放请求体内存
  free(requestBody);
  
  Serial.print("HTTP Response Code: ");
  Serial.println(httpCode);
  
  if (httpCode == 200) {
    // 获取响应体
    response = http.getString();
    Serial.println("Got STT response: " + response);
    
    // 解析JSON响应
    DynamicJsonDocument jsonDoc(1024);
    DeserializationError error = deserializeJson(jsonDoc, response);
    
    if (!error) {
      // 提取转录的文本
      response = jsonDoc["text"].as<String>();
    } else {
      Serial.print("JSON parsing error: ");
      Serial.println(error.c_str());
      response = "";
    }
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(httpCode);
    // 尝试获取错误响应内容
    String errorResponse = http.getString();
    if (errorResponse.length() > 0) {
      Serial.println("Error response: " + errorResponse);
    }
    response = "";
  }
  
  http.end();
  return response;
}
