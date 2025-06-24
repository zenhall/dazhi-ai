<div align="center">

# 🤖 Dazhi-AI

[![Arduino](https://img.shields.io/badge/Arduino-ESP32-blue.svg)](https://github.com/arduino/arduino-esp32)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32-red.svg)](https://www.espressif.com/)

**Serverless AI Voice Assistant | ESP32 Platform | Pure Arduino Development**

English | [简体中文](./README.md)



</div>

## ✨ Table of Contents

- [Project Introduction](#-project-introduction)
- [Key Features](#-key-features)
- [System Architecture](#-system-architecture)
- [Code Description](#-code-description)
- [Hardware Requirements](#-hardware-requirements)
- [Quick Start](#-quick-start)
- [Example Projects](#-example-projects)
- [Community](#-community)

## 📝 Project Introduction

Dazhi-AI is a serverless AI voice assistant developed entirely on the ESP32 platform using the Arduino environment. It allows you to run AI voice interactions directly on ESP32 devices without the need for additional server support. It now supports multimodal capabilities for both voice and visual AI interactions.

## 🚀 Key Features

✅ **Serverless Design**:
- More flexible secondary development
- Higher degree of freedom (customize prompts or models)
- Simpler deployment (no additional server required)

✅ **Two Speech Recognition Options**:
- OpenAI Whisper model (high accuracy)
- iFLYTEK Real-time ASR (low latency)

✅ **Complete Voice Interaction**:
- Voice input
- AI processing
- Voice output

✅ **Multimodal Capabilities**:
- Image recognition
- Visual analysis
- Scene understanding

## 🔧 System Architecture



The system uses a modular design with the following key components:
- Voice Input (Speech-to-Text)
- Visual Input (Camera)
- AI Processing (ChatGPT)
- Voice Output (Text-to-Speech)

## 💻 Code Description

### GPTChatLib Library
Core library files that need to be copied to Arduino's libraries folder.

| Feature | Description |
|---------|-------------|
| ChatGPT Communication | Communicates with OpenAI API, handles requests and responses |
| TTS | Text-to-Speech functionality, converts AI replies to voice |
| STT | Speech-to-Text functionality, converts user input to text |
| Image Recognition | Sends images to GPT for analysis and recognition |
| Audio Processing | Processes and converts various audio data formats |

### Implementation Version Comparison

| Feature | V1 Version (dazhi_v1) | V2 Version (dazhi_v2) |
|---------|----------------------|----------------------|
| Speech Recognition Engine | OpenAI Whisper | iFLYTEK Real-time ASR |
| Recognition Mode | Full recording sent after completion | Real-time recognition while speaking |
| Advantages | Lower API cost | Faster response, lower latency |


### Code Structure
```
├── dazhi_v1/                  # Whisper implementation version
│   └── dazhi_v1.ino           # Main program
├── dazhi_v2/                  # iFLYTEK ASR implementation version
│   └── dazhi_v2.ino           # Main program
├── merged_camera_gpt/         # Camera vision recognition example
│   └── merged_camera_gpt.ino  # Main program
├── calorie_calculator_camera/ # Calorie calculator example
│   └── calorie_calculator_camera.ino # Main program
├── image_recognition_demo/    # Image recognition demo
│   └── image_recognition_demo.ino # Main program
└── GPTChatLib/                # Core functionality library
    ├── ArduinoGPTChat.cpp
    └── ArduinoGPTChat.h
```

## 🔌 Hardware Requirements

### Recommended Hardware
- **Controller**: XIAO ESP32S3
- **Audio Amplifier**: MAX98357A
- **Microphone**: PDM Microphone
- **Camera**: OV2640 (for vision recognition features)
- **Display**: Round Display for XIAO (for visual feedback)

### Pin Connections

| Function | Pin |
|----------|-----|
| I2S_DOUT | 3 |
| I2S_BCLK | 2 |
| I2S_LRC | 1 |
| MIC_DATA | 42 |
| MIC_CLOCK | 41 |
| TOUCH_INT | D7 |

## 🚀 Quick Start

1. **Environment Setup**
   - Install Arduino IDE
   - Install ESP32 board support
   - Install required libraries: ArduinoJson, WiFi, etc.

2. **Library Installation**
   - Copy the GPTChatLib folder to Arduino's libraries directory
   - Install required libraries:
     - ArduinoWebsoket (v0.5.4)
     - ESP32-audioI2S-master (v3.0.13)
       - **Note**: You need to modify the ESP32-audioI2S-master library file:
       - Open `./ESP32-audioI2S-master/src/Audio.cpp`
       - Find: `char host[] = "api.openai.com"`
       - Change it to: `char host[] = "api.chatanywhere.tech"`
     - TFT_eSPI (for display support)
     - SPIFFS (for file system support)

3. **API Key Configuration**
   - Enter your OpenAI API key in the code
   - If using V2 version, enter your iFLYTEK API key

4. **Compile and Upload**
   - Select the appropriate ESP32 development board
   - Compile and upload the code to your device

5. **Testing**
   - Open the serial monitor
   - Follow the prompts for voice or visual interaction

## 📚 Example Projects

### Camera Vision Recognition (merged_camera_gpt)
This example demonstrates how to use the ESP32S3 Sense camera to take photos and send them to GPT for analysis.
- Touch the screen to take a photo
- Automatically send the photo to GPT for analysis
- Display recognition results via serial monitor

### Calorie Calculator (calorie_calculator_camera)
This example implements a vision-based food calorie calculator.
- Take photos of food
- Automatically identify food type
- Estimate food weight and calorie content
- Display analysis results on screen

### Image Recognition Demo (image_recognition_demo)
A simple image recognition demo to test the image recognition capabilities of GPTChatLib.
- Read images from SPIFFS
- Send to GPT for analysis
- Display recognition results

## 💬 Community

Join our WeChat group to share your development experiences and questions:

<div align="center">
  <img src="img/img2.jpg" alt="WeChat Group" width="300"/>
</div>

---

<div align="center">
  <b>Open source collaboration for shared progress!</b><br>
  If you find this project helpful, please give it a ⭐️
</div>
