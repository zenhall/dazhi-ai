#include <WiFi.h>
#include <ArduinoGPTChat.h>
#include "Audio.h"
#include "ESP_I2S.h"
#include <ArduinoWebsockets.h>
#include <mbedtls/md.h>
#include <mbedtls/sha1.h>
#include <ArduinoJson.h>
#include <time.h>

using namespace websockets;

// WiFi设置
const char* ssid = "2nd-curv";
const char* password = "xbotpark";

// 定义I2S引脚（用于音频输出）
#define I2S_DOUT 3
#define I2S_BCLK 2
#define I2S_LRC 1

// 定义麦克风输入引脚
#define MIC_DATA_PIN 42
#define MIC_CLOCK_PIN 41

// NTP服务器设置
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 8 * 3600; // 中国时区 UTC+8
const int   daylightOffset_sec = 0;

// OpenAI API密钥
const char* apiKey = "sk-CkxIb6MfdTBgZkdm0MtUEGVGk6Q6o5X5BRB1DwE2BdeSLSqB";

// 讯飞API设置
const char* app_id = "872c31f2";
const char* api_key = "d54282953514c40782355312881174be";
const char* base_url = "ws://rtasr.xfyun.cn/v1/ws";

// I2S和音频设置
I2SClass I2S;
const int BUFFER_SIZE = 1280; // 每次发送的音频数据大小
int16_t audioBuffer[BUFFER_SIZE]; // 音频缓冲区
unsigned long lastSendTime = 0;
const int SEND_INTERVAL = 40; // 发送间隔，单位毫秒

// 全局声明audio变量，用于TTS播放
Audio audio;

// 初始化ArduinoGPTChat实例
ArduinoGPTChat gptChat(apiKey);

// 状态标志
bool gettingResponse = false;
bool recordingMode = false;
bool wsConnected = false;
bool isConnecting = false;
bool processingGPT = false; // 是否正在处理GPT请求
bool ttsPlaying = false;    // 是否正在播放TTS
unsigned long ttsStartTime = 0; // TTS开始播放的时间
unsigned long ttsTimeout = 30000; // TTS超时时间（30秒）

// 连接控制
unsigned long lastConnectAttemptTime = 0;
unsigned int connectRetryDelay = 30000; // 初始重试延迟30秒
int connectFailCount = 0;
bool limitErrorOccurred = false; // 是否出现过限制错误

// 自动检测句子完成的变量
unsigned long lastResultTime = 0; // 上次接收到识别结果的时间
const unsigned long SILENCE_TIMEOUT = 900; // 沉默超时时间（毫秒），超过这个时间没有新结果则认为句子结束

// WebSocket客户端
WebsocketsClient client;
String currentSentence = "";
String completedSentences[5]; // 存储最近5个已完成的句子
int sentenceCount = 0;

// 函数声明
String calculateSignature(String ts);
void onMessageCallback(WebsocketsMessage message);
void onEventsCallback(WebsocketsEvent event, String data);
String extractTextFromJson(String jsonStr);
String base64Encode(const uint8_t* data, size_t length);
String urlEncode(String str);
void connectToASRServer();
void chatGptCall(String message);
void ttsCall(String text);
void checkTTSPlayback();

void setup() {
  // 初始化串口通信
  Serial.begin(115200);
  delay(1000); // 给串口一些时间来初始化
  
  Serial.println("\n\n----- 语音助手系统启动 -----");
  
  // 连接WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("正在连接WiFi...");
  
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    Serial.print('.');
    delay(1000);
    attempt++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi连接成功！");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
    
    // 配置NTP时间同步
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("正在同步网络时间...");
    
    // 等待时间同步
    int retry = 0;
    while (time(nullptr) < 1600000000 && retry < 10) {
      Serial.print(".");
      delay(1000);
      retry++;
    }
    Serial.println("");
    
    // 显示当前时间
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)){
      Serial.println("当前时间:");
      Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
    } else {
      Serial.println("无法获取时间");
    }
    
    // 设置I2S输出引脚（用于TTS播放）
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    // 设置音量
    audio.setVolume(100);
    
    // 设置WebSocket回调
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    
    // 设置I2S PDM麦克风
    I2S.setPinsPdmRx(MIC_DATA_PIN, MIC_CLOCK_PIN);
    
    // 启动I2S，16kHz采样率，16位采样深度，单声道
    if (!I2S.begin(I2S_MODE_PDM_RX, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
      Serial.println("I2S初始化失败!");
      while (1); // 停止运行
    }
    
    // 连接讯飞WebSocket服务器
    connectToASRServer();
    
    Serial.println("\n----- 系统就绪 -----");
    Serial.println("1. 开始说话，系统会自动检测句子并发送到ChatGPT");
    Serial.println("2. 输入'TTS:文本内容'进行语音合成");
    Serial.println("3. 输入文本直接与ChatGPT对话");
  } else {
    Serial.println("\n连接WiFi失败。请检查网络凭据并重试。");
  }
}

void loop() {
  // 处理音频循环（TTS播放）
  audio.loop();
  
  // 检查TTS播放状态
  checkTTSPlayback();
  
  // 处理串口命令
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.length() > 0) {
      if (command.startsWith("TTS:")) {
        // 文本转语音命令
        String ttsText = command.substring(4);
        ttsText.trim();
        if (ttsText.length() > 0) {
          ttsCall(ttsText);
        } else {
          Serial.println("TTS文本为空。请使用'TTS:你的文本'格式");
        }
      } else {
        // 普通文本作为ChatGPT输入
        Serial.print("用户: ");
        Serial.println(command);
        chatGptCall(command);
      }
    }
  }
  
  // 保持WebSocket连接并处理语音识别
  if (wsConnected) {
    client.poll();
    
    // 只有在不播放TTS时才读取麦克风并发送数据
    if (!ttsPlaying) {
      // 读取音频数据并发送
      readAndSendAudio();
      
      // 检查句子是否完成
      checkSentenceCompletion();
    }
  } 
  else {
    // 检查是否需要尝试重新连接
    unsigned long currentTime = millis();
    
    if (!isConnecting && 
        WiFi.status() == WL_CONNECTED && 
        (currentTime - lastConnectAttemptTime > connectRetryDelay)) {
      
      // 如果之前出现过限制错误，使用更长的延迟
      if (limitErrorOccurred && connectRetryDelay < 120000) {
        connectRetryDelay = 120000; // 2分钟
        Serial.println("因之前出现连接限制错误，采用更长的重连延迟");
      }
      
      Serial.print("尝试重新连接WebSocket服务器... (延迟: ");
      Serial.print(connectRetryDelay / 1000);
      Serial.println("秒)");
      connectToASRServer();
    }
  }
  
  delay(10); // 小延迟以防止CPU过载
}

// 检查TTS是否已完成播放（超时检测）
void checkTTSPlayback() {
  if (ttsPlaying) {
    // 检查是否已经播放超时
    if (millis() - ttsStartTime > ttsTimeout) {
      Serial.println("TTS播放超时，恢复麦克风输入");
      ttsPlaying = false; // 停止TTS播放标志
    }
    
    // 检查音频是否还在播放
    if (!audio.isRunning()) {
      Serial.println("TTS播放完成，恢复麦克风输入");
      ttsPlaying = false; // 停止TTS播放标志
    }
  }
}

// 连接讯飞ASR服务器
void connectToASRServer() {
  isConnecting = true;
  lastConnectAttemptTime = millis();
  
  // 获取当前时间戳
  time_t now;
  time(&now);
  
  // 检查时间戳是否有效
  if (now < 1600000000 || now > 2500000000) {
    Serial.println("警告: 系统时间可能不正确，尝试重新同步NTP时间");
    
    // 重新同步NTP时间
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(2000);
    time(&now);
    
    // 如果仍然无效，使用当前时间
    if (now < 1600000000 || now > 2500000000) {
      Serial.println("无法获取有效时间，使用当前时间");
      randomSeed(millis());
      now = 1747232100 + random(100, 999); // 使用2025年5月的时间戳
    }
  } else {
    // 添加一些随机性，避免每次使用完全相同的时间戳
    randomSeed(millis());
    now += random(1, 100);
  }
  
  String ts = String(now);
  Serial.println("使用时间戳: " + ts);
  
  // 计算签名
  String signature = calculateSignature(ts);
  
  // 构建URL
  String url = String(base_url) + "?appid=" + app_id + "&ts=" + ts + "&signa=" + urlEncode(signature);
  
  Serial.println("正在连接讯飞WebSocket服务器...");
  Serial.println("URL: " + url);
  
  // 关闭任何可能存在的旧连接
  if (client.available()) {
    client.close();
    delay(1000); // 等待连接完全关闭
  }
  
  // 尝试连接
  bool connected = client.connect(url);
  if (connected) {
    Serial.println("WebSocket连接成功!");
    wsConnected = true;
    connectFailCount = 0; // 重置失败计数
  } else {
    Serial.println("WebSocket连接失败!");
    wsConnected = false;
    connectFailCount++;
    
    // 实现指数退避策略，但因为接口限制，增加最小重试时间
    if (connectRetryDelay < 30000) {
      connectRetryDelay = 30000; // 至少30秒
    } else if (connectFailCount > 2) {
      connectRetryDelay = min(300000U, connectRetryDelay * 2); // 最长5分钟
    }
  }
  
  isConnecting = false;
}

// 读取并发送音频数据
void readAndSendAudio() {
  // 如果TTS正在播放，不读取麦克风
  if (ttsPlaying) {
    return;
  }
  
  unsigned long currentTime = millis();
  
  // 每40ms发送一次音频数据
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = currentTime;
    
    // 读取音频数据到缓冲区
    int samplesRead = 0;
    while (samplesRead < BUFFER_SIZE) {
      int sample = I2S.read();
      if (sample != 0 && sample != -1 && sample != 1) {
        audioBuffer[samplesRead++] = sample;
      }
    }
    
    // 发送音频数据
    if (wsConnected) {
      client.sendBinary((const char*)audioBuffer, BUFFER_SIZE * 2); // 16位每样本，所以乘以2
    }
  }
}

// 发送结束标志
void sendEndTag() {
  if (wsConnected) {
    String endTag = "{\"end\": true}";
    client.send(endTag);
    Serial.println("已发送结束标签");
  }
}

// Base64编码函数
String base64Encode(const uint8_t* data, size_t length) {
  static const char* base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  
  size_t outputLength = 4 * ((length + 2) / 3);
  char* encoded = new char[outputLength + 1];
  
  size_t i = 0, j = 0;
  uint8_t char_array_3[3];
  uint8_t char_array_4[4];
  
  while (length--) {
    char_array_3[i++] = *(data++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;
      
      for (i = 0; i < 4; i++) {
        encoded[j++] = base64_chars[char_array_4[i]];
      }
      i = 0;
    }
  }
  
  if (i) {
    for (size_t k = i; k < 3; k++) {
      char_array_3[k] = '\0';
    }
    
    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    
    for (size_t k = 0; k < i + 1; k++) {
      encoded[j++] = base64_chars[char_array_4[k]];
    }
    
    while (i++ < 3) {
      encoded[j++] = '=';
    }
  }
  
  encoded[j] = '\0';
  String result = String(encoded);
  delete[] encoded;
  
  return result;
}

// 计算签名
String calculateSignature(String ts) {
  // 计算md5
  uint8_t md5Result[16];
  String baseString = app_id + ts;
  mbedtls_md_context_t md5Ctx;
  mbedtls_md_init(&md5Ctx);
  mbedtls_md_setup(&md5Ctx, mbedtls_md_info_from_type(MBEDTLS_MD_MD5), 0);
  mbedtls_md_starts(&md5Ctx);
  mbedtls_md_update(&md5Ctx, (const unsigned char*)baseString.c_str(), baseString.length());
  mbedtls_md_finish(&md5Ctx, md5Result);
  mbedtls_md_free(&md5Ctx);
  
  // 将md5结果转换为十六进制字符串
  char md5String[33];
  for (int i = 0; i < 16; i++) {
    sprintf(&md5String[i * 2], "%02x", md5Result[i]);
  }
  md5String[32] = '\0';
  
  // 计算HMAC-SHA1签名
  uint8_t hmacResult[20];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)api_key, strlen(api_key));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)md5String, 32);
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  
  // 使用自定义Base64编码
  return base64Encode(hmacResult, sizeof(hmacResult));
}

// URL编码
String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

// 检查句子是否完成
void checkSentenceCompletion() {
  // 如果TTS正在播放，不检查句子完成
  if (ttsPlaying) {
    return;
  }
  
  // 当前时间
  unsigned long currentTime = millis();
  
  // 如果有当前正在处理的句子，检查是否超时
  if (currentSentence.length() > 0 && (currentTime - lastResultTime > SILENCE_TIMEOUT)) {
    // 句子已超时，认为一句话已结束
    Serial.println("\n【检测到一句话已说完!】");
    updateCompletedSentences();
  }
}

// 更新已完成的句子列表 并发送到GPT
void updateCompletedSentences() {
  if (currentSentence.length() > 0 && !processingGPT) {
    // 将当前句子添加到已完成句子列表
    completedSentences[sentenceCount % 5] = currentSentence;
    String completedSentence = currentSentence; // 保存当前完成的句子用于发送
    sentenceCount++;
    currentSentence = "";
    
    // 显示已完成的句子
    Serial.println("\n===== 已完成的句子 =====");
    for (int i = 0; i < min(sentenceCount, 5); i++) {
      int index = (sentenceCount - i - 1) % 5;
      Serial.print(i + 1);
      Serial.print(". ");
      Serial.println(completedSentences[index]);
    }
    Serial.println("========================\n");
    
    // 发送最新完成的句子到ChatGPT
    Serial.println("正在将句子发送到ChatGPT: " + completedSentence);
    processingGPT = true; // 设置处理状态，防止重复发送
    chatGptCall(completedSentence);
    processingGPT = false;
  }
}

// WebSocket消息回调
void onMessageCallback(WebsocketsMessage message) {
  // 如果TTS正在播放，不处理ASR消息
  if (ttsPlaying) {
    return;
  }
  
  String payload = message.data();
  Serial.println("收到消息: " + payload);
  
  // 更新最后接收结果的时间
  lastResultTime = millis();
  
  // 解析JSON响应
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, payload);
  
  if (error) {
    Serial.print("JSON解析失败: ");
    Serial.println(error.c_str());
    return;
  }
  
  // 处理不同类型的响应
  const char* action = doc["action"];
  
  if (strcmp(action, "started") == 0) {
    Serial.println("识别开始");
    // 重置限制错误标志
    limitErrorOccurred = false;
  } 
  else if (strcmp(action, "result") == 0) {
    // 提取识别文本
    String text = extractTextFromJson(payload);
    if (text.length() > 0) {
      currentSentence = text;
      Serial.println("当前识别: " + currentSentence);
      // 更新最后接收结果的时间
      lastResultTime = millis();
    }
  } 
  else if (strcmp(action, "error") == 0) {
    String errorCode = doc["code"].as<String>();
    String errorDesc = doc["desc"].as<String>();
    
    Serial.print("识别错误 [");
    Serial.print(errorCode);
    Serial.print("]: ");
    Serial.println(errorDesc);
    
    wsConnected = false;
    
    // 处理特定错误
    if (errorCode == "10800") {
      Serial.println("连接限制错误，增加重连延迟");
      limitErrorOccurred = true;
      connectRetryDelay = 120000; // 设置为2分钟
    } 
    else if (errorCode == "10105") {
      Serial.println("时间戳过期错误，强制重新同步时间");
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }
  }
}

// WebSocket事件回调
void onEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    Serial.println("WebSocket连接已打开");
    wsConnected = true;
  } 
  else if (event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("WebSocket连接已关闭");
    wsConnected = false;
  }
}

// 从JSON中提取文本
String extractTextFromJson(String jsonStr) {
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonStr);
  
  if (error) {
    Serial.print("JSON解析失败: ");
    Serial.println(error.c_str());
    return "";
  }
  
  // 检查是否包含data字段
  if (!doc.containsKey("data")) {
    return "";
  }
  
  String data = doc["data"];
  if (data.length() == 0) {
    return "";
  }
  
  // 尝试解析内部JSON
  DynamicJsonDocument innerDoc(1024);
  error = deserializeJson(innerDoc, data);
  
  if (error) {
    // 如果解析内部JSON失败，直接返回data
    return data;
  }
  
  // 提取文本
  if (!innerDoc.containsKey("cn")) {
    return "";
  }
  
  JsonObject cn = innerDoc["cn"];
  if (!cn.containsKey("st")) {
    return "";
  }
  
  JsonObject st = cn["st"];
  if (!st.containsKey("rt")) {
    return "";
  }
  
  JsonArray rt = st["rt"];
  String text = "";
  
  for (JsonVariant rtItem : rt) {
    if (!rtItem.containsKey("ws")) {
      continue;
    }
    
    JsonArray ws = rtItem["ws"];
    for (JsonVariant wsItem : ws) {
      if (!wsItem.containsKey("cw")) {
        continue;
      }
      
      JsonArray cw = wsItem["cw"];
      for (JsonVariant cwItem : cw) {
        if (cwItem.containsKey("w")) {
          text += String(cwItem["w"].as<const char*>());
        }
      }
    }
  }
  
  return text;
}

// 发送消息到ChatGPT
void chatGptCall(String message) {
  Serial.println("正在向ChatGPT发送请求...");
  gettingResponse = true;
  
  String response = gptChat.sendMessage(message);
  
  if (response != "") {
    Serial.print("ChatGPT: ");
    Serial.println(response);
    
    // 自动将回复转换为语音
    if (response.length() > 0) {
      ttsCall(response);
    }
    gettingResponse = false;
  } else {
    Serial.println("获取ChatGPT响应失败");
    gettingResponse = false;
  }
}

// 文本转语音
void ttsCall(String text) {
  Serial.println("正在将文本转换为语音...");
  
  // 在TTS开始播放前设置标志
  ttsPlaying = true;
  ttsStartTime = millis(); // 记录开始时间用于超时检测
  
  // 使用GPTChat的textToSpeech方法
  bool success = gptChat.textToSpeech(text);
  
  if (success) {
    Serial.println("TTS音频正在通过扬声器播放");
    Serial.println("麦克风输入已临时禁用");
  } else {
    Serial.println("播放TTS音频失败");
    ttsPlaying = false; // 如果播放失败，重置标志
  }
}
