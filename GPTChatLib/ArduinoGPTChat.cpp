#include "ArduinoGPTChat.h"
#include <SPIFFS.h>

// Base64编码表
const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void ArduinoGPTChat::base64_encode(const uint8_t* input, size_t length, char* output) {
  size_t i = 0, j = 0;
  uint8_t char_array_3[3];
  uint8_t char_array_4[4];

  while (length--) {
    char_array_3[i++] = *(input++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; i < 4; i++)
        output[j++] = base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for(size_t k = i; k < 3; k++)
      char_array_3[k] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (size_t k = 0; k < i + 1; k++)
      output[j++] = base64_chars[char_array_4[k]];

    while(i++ < 3)
      output[j++] = '=';
  }
  output[j] = '\0';
}

size_t ArduinoGPTChat::base64_encode_length(size_t input_length) {
  size_t ret_size = input_length;
  if (ret_size % 3 != 0)
    ret_size += 3 - (ret_size % 3);
  ret_size /= 3;
  ret_size *= 4;
  return ret_size + 1;  // +1 for null terminator
}

String ArduinoGPTChat::sendImageMessage(const char* imageFilePath, String question) {
  Serial.println("Opening image file...");
  File imageFile = SPIFFS.open(imageFilePath);
  if (!imageFile) {
    Serial.println("Failed to open image file");
    return "Error: Failed to open image file";
  }

  size_t fileSize = imageFile.size();
  Serial.printf("File size: %d bytes\n", fileSize);
  
  // 方案2：使用flash空间存储base64编码数据
  const char* tempBase64File = "/temp_base64.txt";
  
  // 删除可能存在的临时文件
  if (SPIFFS.exists(tempBase64File)) {
    SPIFFS.remove(tempBase64File);
  }
  
  // 创建临时文件用于存储base64数据
  File base64File = SPIFFS.open(tempBase64File, FILE_WRITE);
  if (!base64File) {
    Serial.println("Failed to create temp base64 file");
    imageFile.close();
    return "Error: Failed to create temp file";
  }
  
  // 写入base64前缀
  base64File.print("data:image/png;base64,");
  
  Serial.println("Starting base64 encoding and writing to SD...");
  
  // 使用更小的块大小和动态内存分配，避免栈溢出
  const size_t chunkSize = 1500; // 减小到1.5KB chunks (must be multiple of 3 for base64)
  
  uint8_t* buffer = (uint8_t*)malloc(chunkSize);
  if (!buffer) {
    Serial.println("Failed to allocate buffer memory");
    imageFile.close();
    base64File.close();
    SPIFFS.remove(tempBase64File);
    return "Error: Failed to allocate buffer";
  }
  
  char* encodedChunk = (char*)malloc(chunkSize * 4 / 3 + 10); // base64 expansion + padding
  if (!encodedChunk) {
    Serial.println("Failed to allocate encoded buffer memory");
    free(buffer);
    imageFile.close();
    base64File.close();
    SPIFFS.remove(tempBase64File);
    return "Error: Failed to allocate encoded buffer";
  }
  
  size_t totalProcessed = 0;
  while (totalProcessed < fileSize) {
    size_t currentChunkSize = min(chunkSize, fileSize - totalProcessed);
    size_t bytesRead = imageFile.read(buffer, currentChunkSize);
    
    if (bytesRead != currentChunkSize) {
      Serial.println("Error reading file chunk");
      free(buffer);
      free(encodedChunk);
      imageFile.close();
      base64File.close();
      SPIFFS.remove(tempBase64File);
      return "Error: Failed to read file chunk";
    }
    
    // 编码当前块
    base64_encode(buffer, bytesRead, encodedChunk);
    
    // 写入编码数据到SPIFFS卡
    base64File.print(encodedChunk);
    
    totalProcessed += bytesRead;
    
    // 显示进度
    if (totalProcessed % 15000 == 0 || totalProcessed == fileSize) {
      Serial.printf("Processed: %d/%d bytes (%.1f%%)\n", 
                   totalProcessed, fileSize, 
                   (float)totalProcessed / fileSize * 100);
    }
    
    // 添加小延迟，让系统有时间处理其他任务
    delay(10);
  }
  
  // 释放缓冲区内存
  free(buffer);
  free(encodedChunk);
  
  imageFile.close();
  base64File.close();
  
  Serial.println("Base64 encoding completed and saved to SPIFFS");
  
  // 检查生成的base64文件大小
  File checkFile = SPIFFS.open(tempBase64File);
  if (!checkFile) {
    Serial.println("Failed to open temp base64 file for reading");
    return "Error: Failed to open temp base64 file";
  }
  
  size_t base64FileSize = checkFile.size();
  Serial.printf("Base64 file size: %d bytes\n", base64FileSize);
  checkFile.close();
  
  // 现在构建JSON，使用较小的缓冲区
  DynamicJsonDocument doc(2048); // 只需要小缓冲区，因为不包含base64数据
  doc["model"] = "gpt-4.1-nano";
  doc["messages"] = JsonArray();
  JsonObject message = doc["messages"].createNestedObject();
  message["role"] = "user";
  JsonArray content = message.createNestedArray("content");
  
  // 添加文本部分
  JsonObject textPart = content.createNestedObject();
  textPart["type"] = "text";
  textPart["text"] = question;
  
  // 添加图片部分
  JsonObject imagePart = content.createNestedObject();
  imagePart["type"] = "image_url";
  JsonObject imageUrl = imagePart.createNestedObject("image_url");
  
  // 设置占位符，稍后替换
  imageUrl["url"] = "PLACEHOLDER_FOR_BASE64_DATA";
  
  doc["max_tokens"] = 300;
  
  // 序列化JSON到字符串
  String jsonTemplate;
  serializeJson(doc, jsonTemplate);
  
  Serial.println("JSON template created");
  Serial.printf("Template size: %d bytes\n", jsonTemplate.length());
  
  // 构建完整JSON，但分块读取base64数据以节省内存
  Serial.println("Building JSON with chunked base64 reading...");
  
  // 计算最终JSON大小
  int placeholderPos = jsonTemplate.indexOf("PLACEHOLDER_FOR_BASE64_DATA");
  String jsonPart1 = jsonTemplate.substring(0, placeholderPos);
  String jsonPart2 = jsonTemplate.substring(placeholderPos + strlen("PLACEHOLDER_FOR_BASE64_DATA"));
  
  Serial.printf("JSON part 1 size: %d bytes\n", jsonPart1.length());
  Serial.printf("JSON part 2 size: %d bytes\n", jsonPart2.length());
  Serial.printf("Base64 data size: %d bytes\n", base64FileSize);
  
  // 创建一个临时文件来存储完整的JSON
  const char* tempJsonFile = "/temp_json.txt";
  if (SPIFFS.exists(tempJsonFile)) {
    SPIFFS.remove(tempJsonFile);
  }
  
  File jsonFile = SPIFFS.open(tempJsonFile, FILE_WRITE);
  if (!jsonFile) {
    Serial.println("Failed to create temp JSON file");
    SPIFFS.remove(tempBase64File);
    return "Error: Failed to create temp JSON file";
  }
  
  // 写入JSON第一部分
  jsonFile.print(jsonPart1);
  
  // 分块读取base64数据并写入JSON文件
  Serial.println("Copying base64 data to JSON file...");
  File base64ReadFile = SPIFFS.open(tempBase64File);
  if (!base64ReadFile) {
    Serial.println("Failed to open base64 file for reading");
    jsonFile.close();
    SPIFFS.remove(tempBase64File);
    SPIFFS.remove(tempJsonFile);
    return "Error: Failed to read base64 file";
  }
  
  const size_t copyChunkSize = 2048;
  uint8_t* copyBuffer = (uint8_t*)malloc(copyChunkSize);
  if (!copyBuffer) {
    Serial.println("Failed to allocate copy buffer");
    base64ReadFile.close();
    jsonFile.close();
    SPIFFS.remove(tempBase64File);
    SPIFFS.remove(tempJsonFile);
    return "Error: Failed to allocate copy buffer";
  }
  
  size_t totalCopied = 0;
  while (base64ReadFile.available()) {
    size_t bytesToRead = min(copyChunkSize, (size_t)base64ReadFile.available());
    size_t bytesRead = base64ReadFile.read(copyBuffer, bytesToRead);
    
    jsonFile.write(copyBuffer, bytesRead);
    totalCopied += bytesRead;
    
    if (totalCopied % 20480 == 0) { // 每20KB显示一次进度
      Serial.printf("Copied: %d/%d bytes\n", totalCopied, base64FileSize);
    }
  }
  
  free(copyBuffer);
  base64ReadFile.close();
  
  // 写入JSON第二部分
  jsonFile.print(jsonPart2);
  jsonFile.close();
  
  // 清理base64临时文件
  SPIFFS.remove(tempBase64File);
  
  // 检查JSON文件大小
  File checkJsonFile = SPIFFS.open(tempJsonFile);
  if (!checkJsonFile) {
    Serial.println("Failed to open JSON file for size check");
  SPIFFS.remove(tempJsonFile);
    return "Error: Failed to open JSON file";
  }
  
  size_t jsonFileSize = checkJsonFile.size();
  Serial.printf("Final JSON file size: %d bytes\n", jsonFileSize);
  checkJsonFile.close();
  
  // 使用真正的流式HTTP发送，避免将大JSON加载到内存
  Serial.println("Starting streaming HTTP POST...");
  
  WiFiClientSecure client;
  client.setInsecure(); // 跳过SSL证书验证
  
  if (!client.connect("api.chatanywhere.tech", 443)) {
    Serial.println("Failed to connect to server");
    SPIFFS.remove(tempJsonFile);
    return "Error: Failed to connect to server";
  }
  
  // 发送HTTP请求头
  client.print("POST /v1/chat/completions HTTP/1.1\r\n");
  client.print("Host: api.chatanywhere.tech\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Authorization: Bearer ");
  client.print(_apiKey);
  client.print("\r\n");
  client.print("Content-Length: ");
  client.print(jsonFileSize);
  client.print("\r\n");
  client.print("Connection: close\r\n");
  client.print("\r\n");
  
  Serial.println("Streaming JSON file...");
  
  // 流式发送JSON文件
  File sendJsonFile = SPIFFS.open(tempJsonFile);
  if (!sendJsonFile) {
    Serial.println("Failed to open JSON file for streaming");
    client.stop();
    SPIFFS.remove(tempJsonFile);
    return "Error: Failed to open JSON file for streaming";
  }
  
  const size_t streamChunkSize = 1024;
  uint8_t* streamBuffer = (uint8_t*)malloc(streamChunkSize);
  if (!streamBuffer) {
    Serial.println("Failed to allocate stream buffer");
    sendJsonFile.close();
    client.stop();
    SPIFFS.remove(tempJsonFile);
    return "Error: Failed to allocate stream buffer";
  }
  
  size_t totalStreamed = 0;
  while (sendJsonFile.available()) {
    size_t bytesToRead = min(streamChunkSize, (size_t)sendJsonFile.available());
    size_t bytesRead = sendJsonFile.read(streamBuffer, bytesToRead);
    
    client.write(streamBuffer, bytesRead);
    totalStreamed += bytesRead;
    
    if (totalStreamed % 10240 == 0) { // 每10KB显示一次进度
      Serial.printf("Streamed: %d/%d bytes\n", totalStreamed, jsonFileSize);
    }
    
    delay(1); // 小延迟确保数据发送稳定
  }
  
  free(streamBuffer);
  sendJsonFile.close();
  SPIFFS.remove(tempJsonFile);
  
  Serial.printf("Total streamed: %d bytes\n", totalStreamed);
  Serial.println("Waiting for response...");
  
  // 等待响应
  unsigned long timeout = millis() + 30000; // 30秒超时
  while (!client.available() && millis() < timeout) {
    delay(100);
  }
  
  if (millis() >= timeout) {
    Serial.println("HTTP response timeout");
    client.stop();
    return "Error: HTTP response timeout";
  }
  
  // 读取响应
  String response = "";
  String line = "";
  bool headersPassed = false;
  int statusCode = 0;
  
  Serial.println("Reading response headers...");
  
  while (client.available()) {
    char c = client.read();
    if (c == '\n') {
      if (line.length() <= 1) { // 空行或只有\r，表示头部结束
        if (!headersPassed) {
          headersPassed = true;
          Serial.println("Headers passed, reading body...");
        }
        line = "";
        continue;
      }
      if (headersPassed) {
        response += line + "\n";
      } else {
        // 解析响应头
        if (line.startsWith("HTTP/1.1 ")) {
          statusCode = line.substring(9, 12).toInt();
          Serial.printf("HTTP Response code: %d\n", statusCode);
        }
        Serial.println("Header: " + line);
      }
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }
  
  // 处理最后一行
  if (line.length() > 0) {
    if (headersPassed) {
      response += line;
    } else {
      Serial.println("Last header: " + line);
    }
  }
  
  client.stop();
  
  if (statusCode == 200 && response.length() > 0) {
    Serial.println("Raw response:");
    Serial.println(response);
    
    // 处理分块传输编码 - 查找JSON开始位置
    String cleanResponse = response;
    int jsonStart = cleanResponse.indexOf('{');
    if (jsonStart > 0) {
      cleanResponse = cleanResponse.substring(jsonStart);
      Serial.println("Cleaned JSON response:");
      Serial.println(cleanResponse);
    }
    
    return _processResponse(cleanResponse);
  } else {
    Serial.println("Error response:");
    Serial.println(response);
    return "Error: HTTP request failed with code " + String(statusCode);
  }
}

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
