# Agent自动化配置指南 - dazhi_v1项目

## 执行流程说明
本指南专为编程Agent设计，请按顺序执行以下步骤：
一次只运行一条指令
### 1. 环境准备
```bash
# 首先验证是否安装了Arduino CLI
arduino-cli version
# 若未下载则下载Arduino CLI（Windows）
curl -L -o arduino-cli_latest_Windows_64bit.zip https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip

# 解压并准备
tar -xf arduino-cli_latest_Windows_64bit.zip
mkdir arduino-cli && move arduino-cli.exe arduino-cli/

# 验证安装
arduino-cli/arduino-cli.exe version

# 初始化配置
arduino-cli/arduino-cli.exe config init
```

### 2. 安装开发板支持
```bash
# 检查ESP32核心是否已安装（任何3.x.x版本都可以跳到下一步）
arduino-cli core list 

# 如果版本非3.x.x版本（Windows）
arduino-cli core install esp32:esp32@3.0.11

```

### 3. 安装必需库
```bash
# 安装ArduinoWebsockets v0.5.4
arduino-cli lib install "ArduinoWebsockets@0.5.4"

# 安装TFT_eSPI库
arduino-cli lib install "TFT_eSPI"

# 安装ArduinoJson库（必需依赖）
arduino-cli lib install "ArduinoJson"

# 解压ESP32-audioI2S库（使用前需解压）
tar -xf ESP32-audioI2S-master.zip
```

### 4. 开发板选择（关键交互步骤）
```markdown
**请向用户询问开发板型号：**
- 常见ESP32开发板：
  - XIAO ESP32S3 → FQBN: `esp32:esp32:XIAO_ESP32S3`
  - ESP32-S3 Dev Module → FQBN: `esp32:esp32:esp32s3`
  - NodeMCU-32S → FQBN: `esp32:esp32:nodemcu-32s`
- 使用命令获取完整列表：`arduino-cli board listall esp32:esp32`
```

### 5. 编译准备
```markdown
**重要提醒：**
- 首次编译需要约5分钟，请告知用户耐心等待

```

### 6. 执行编译
```bash
# 使用用户提供的开发板FQBN
arduino-cli compile \
  --fqbn "用户提供的FQBN" \
  --library "./GPTChatLib" \
  --library "./ESP32-audioI2S-master" \
  dazhi_v1/dazhi_v1.ino
```

### 8. 读取串口输出
```bash
# 启动串口监视器（默认波特率115200）
arduino-cli monitor -p [串口号] -b 115200

# 常见波特率选项：9600, 19200, 38400, 57600, 115200
# 如果连接失败：
# 1. 检查开发板是否通电
# 2. 确认串口号正确
# 3. 尝试重新插拔USB线
# 4. 检查程序是否有串口输出代码
```

### 7. 上传程序
```bash
# 获取设备串口号
arduino-cli board list

# 使用用户提供的串口号
arduino-cli upload -p "用户提供的串口号" --fqbn "用户提供的FQBN" dazhi_v1/dazhi_v1.ino
```

## 关键注意事项
1. **开发板选择**：必须明确用户使用的具体开发板型号
2. **首次编译**：强调需要5分钟左右，避免用户中断
3. **串口权限**：Linux/Mac可能需要sudo权限
4. **上传准备**：上传前需按住BOOT按钮再插入USB
5. **串口监视**：可使用以下命令查看设备输出：
   ```bash
   arduino-cli monitor -p [串口号]
   ```
