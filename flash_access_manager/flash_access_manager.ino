#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>

const char* ssid = "2nd-curv";
const char* password = "xbotpark";

WebServer server(80);

void setup() {
  Serial.begin(115200);
  
  // 初始化SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  // 连接WiFi
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());

  // 设置mDNS
  if(!MDNS.begin("esp32")) {
    Serial.println("Error setting up MDNS responder!");
  }

  // Web服务器路由
  server.on("/", HTTP_GET, [](){
    String fileList = "<h1>Flash Files</h1><ul>";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while(file){
      fileList += "<li><a href='/download?file=" + String(file.name()) + "'>" + String(file.name()) + "</a>";
      fileList += " (" + String(file.size()) + " bytes)";
      fileList += " <a href='/delete?file=" + String(file.name()) + "' style='color:red'>[Delete]</a></li>";
      file = root.openNextFile();
    }
    fileList += "</ul>";
    server.send(200, "text/html", fileList);
  });

  server.on("/download", HTTP_GET, [](){
    if(server.hasArg("file")){
      String filename = "/" + server.arg("file");
      if(SPIFFS.exists(filename)){
        File file = SPIFFS.open(filename);
        server.sendHeader("Content-Type", "application/octet-stream");
        server.sendHeader("Content-Disposition", "attachment; filename=" + server.arg("file"));
        server.sendHeader("Connection", "close");
        server.streamFile(file, "application/octet-stream");
        file.close();
      }
    }
  });

  server.on("/delete", HTTP_GET, [](){
    if(server.hasArg("file")){
      String filename = "/" + server.arg("file");
      if(SPIFFS.exists(filename)){
        SPIFFS.remove(filename);
        server.sendHeader("Location", "/");
        server.send(303);
      }
    }
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // USB串口命令处理
  if(Serial.available()){
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if(command == "list"){
      File root = SPIFFS.open("/");
      File file = root.openNextFile();
      while(file){
        Serial.printf("%s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
      }
    }
    else if(command.startsWith("download ")){
      String filename = command.substring(9);
      filename.trim();
      if(SPIFFS.exists("/" + filename)){
        File file = SPIFFS.open("/" + filename);
        Serial.write(file.read());
        file.close();
      } else {
        Serial.println("File not found");
      }
    }
    else if(command.startsWith("delete ")){
      String filename = command.substring(7);
      filename.trim();
      if(SPIFFS.exists("/" + filename)){
        SPIFFS.remove("/" + filename);
        Serial.println("File deleted");
      } else {
        Serial.println("File not found");
      }
    }
    else {
      Serial.println("Available commands:");
      Serial.println("list - List all files");
      Serial.println("download [filename] - Download file");
      Serial.println("delete [filename] - Delete file");
    }
  }
}
