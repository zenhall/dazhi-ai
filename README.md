<div align="center">

# 🤖 大智AI (Dazhi-AI)

[![Arduino](https://img.shields.io/badge/Arduino-ESP32-blue.svg)](https://github.com/arduino/arduino-esp32)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32-red.svg)](https://www.espressif.com/)

**无服务端 AI 语音助手 | ESP32 平台 | 纯 Arduino 开发**

[English](./README_EN.md) | 简体中文



</div>

## ✨ 目录

- [项目简介](#-项目简介)
- [主要特点](#-主要特点)
- [系统架构](#-系统架构)
- [代码说明](#-代码说明)
- [硬件需求](#-硬件需求)
- [快速开始](#-快速开始)
- [示例项目](#-示例项目)
- [交流讨论](#-交流讨论)

## 📝 项目简介

大智AI是一个完全基于ESP32开发的无服务器AI语音助手，采用纯Arduino开发环境。它允许您直接在ESP32设备上运行AI语音交互，无需额外的服务器支持。现已支持多模态能力，可实现语音和视觉的AI交互。

## 🚀 主要特点

✅ **无服务端**：
- 更灵活的二次开发机会
- 更高的自由度（可自定义修改prompt或模型）
- 更简单的部署流程（无需运行额外服务器）

✅ **两种语音识别方案**：
- Whisper模型识别 (高准确率)
- 科大讯飞实时ASR (低延迟)

✅ **完整语音交互**：
- 语音输入
- AI处理
- 语音输出

✅ **多模态能力**：
- 图像识别
- 视觉分析
- 场景理解

## 🔧 系统架构



系统采用模块化设计，主要分为以下几个功能模块：
- 语音输入 (Speech-to-Text)
- 视觉输入 (Camera)
- AI处理 (ChatGPT)
- 语音输出 (Text-to-Speech)

## 💻 代码说明

### GPTChatLib库
核心库文件，需要拷贝到Arduino的libraries文件夹中。

| 功能 | 描述 |
|------|------|
| ChatGPT通信 | 与OpenAI API进行通信，处理请求和响应 |
| TTS | 文本转语音功能，将AI回复转为语音 |
| STT | 语音转文本功能，将用户输入转为文本 |
| 图像识别 | 发送图像到GPT进行分析和识别 |
| 音频处理 | 处理和转换各种音频数据格式 |

### 实现版本对比

| 特性 | V1版本 (dazhi_v1) | V2版本 (dazhi_v2) |
|------|------------------|------------------|
| 语音识别引擎 | OpenAI Whisper | 科大讯飞实时ASR |
| 识别模式 | 录音完成后整段发送 | 实时边说边识别 |
| 优势 | API价格低 | 响应速度快，延迟更低 |


### 代码结构
```
├── dazhi_v1/                  # Whisper实现版本
│   └── dazhi_v1.ino           # 主程序
├── dazhi_v2/                  # 讯飞ASR实现版本
│   └── dazhi_v2.ino           # 主程序
├── merged_camera_gpt/         # 摄像头视觉识别示例
│   └── merged_camera_gpt.ino  # 主程序
├── calorie_calculator_camera/ # 卡路里计算器示例
│   └── calorie_calculator_camera.ino # 主程序
├── image_recognition_demo/    # 图像识别演示
│   └── image_recognition_demo.ino # 主程序
└── GPTChatLib/                # 核心功能库
    ├── ArduinoGPTChat.cpp
    └── ArduinoGPTChat.h
```

## 🔌 硬件需求

### 推荐硬件
- **主控板**：XIAO ESP32S3
- **音频放大器**：MAX98357A
- **麦克风**：PDM麦克风
- **摄像头**：OV2640（用于视觉识别功能）
- **显示屏**：Round Display for XIAO（用于视觉反馈）

### 引脚连接

| 功能 | 引脚 |
|------|------|
| I2S_DOUT | 3 |
| I2S_BCLK | 2 |
| I2S_LRC | 1 |
| MIC_DATA | 42 |
| MIC_CLOCK | 41 |
| TOUCH_INT | D7 |

## 🚀 快速开始

1. **环境准备**
   - 安装 Arduino IDE
   - 安装 ESP32 开发板支持
   - 安装必要的库：ArduinoJson, WiFi等

2. **库文件安装**
   - 将GPTChatLib文件夹复制到Arduino的libraries目录
   - 安装必要的库：
     - ArduinoWebsoket (v0.5.4)
     - ESP32-audioI2S-master (v3.0.13)
       - **注意**：需要修改ESP32-audioI2S-master库文件：
       - 打开 `./ESP32-audioI2S-master/src/Audio.cpp`
       - 查找：`char host[] = "api.openai.com"`
       - 将其修改为：`char host[] = "api.chatanywhere.tech"`
     - TFT_eSPI（用于显示屏支持）
     - SPIFFS（用于文件系统支持）

3. **配置API密钥**
   - 在代码中填入您的OpenAI API密钥
   - 如使用V2版本，需填入科大讯飞的API密钥

4. **编译上传**
   - 选择合适的ESP32开发板
   - 编译并上传代码到设备

5. **测试使用**
   - 打开串口监视器
   - 按提示进行语音或视觉交互

## 📚 示例项目

### 摄像头视觉识别 (merged_camera_gpt)
这个示例展示了如何使用ESP32S3 Sense的摄像头拍照，并将图像发送给GPT进行分析识别。
- 触摸屏幕拍照
- 自动将照片发送给GPT进行分析
- 通过串口显示识别结果

### 卡路里计算器 (calorie_calculator_camera)
这个示例实现了一个基于视觉识别的食物卡路里计算器。
- 拍摄食物照片
- 自动识别食物类型
- 估算食物重量和卡路里含量
- 在屏幕上显示分析结果

### 图像识别演示 (image_recognition_demo)
简单的图像识别演示，用于测试GPTChatLib的图像识别功能。
- 从SPIFFS读取图像
- 发送给GPT进行分析
- 显示识别结果

## 💬 交流讨论

欢迎加入我们的微信交流群，分享您的开发经验和问题：

<div align="center">
  <img src="img/img2.jpg" alt="微信群" width="300"/>
</div>

---

<div align="center">
  <b>开源协作，共同进步！</b><br>
  如果您觉得这个项目有帮助，请给它一个⭐️
</div>
