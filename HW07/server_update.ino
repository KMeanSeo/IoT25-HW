#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define LED_R 25
#define LED_G 26
#define LED_B 27

WebServer server(80);
const char* ssid = "your_wifi_ssid";
const char* password = "your_wifi_password";

void setup() {
  Serial.begin(115200);
  
  // LED 핀 설정
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  analogWrite(LED_R, 0);
  analogWrite(LED_G, 0);
  analogWrite(LED_B, 0);

  // BLE 서버 초기화
  BLEDevice::init("Distance_Server");
  BLEServer *pServer = BLEDevice::createServer();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();

  // WiFi 연결
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("WiFi Connected. IP: " + WiFi.localIP());

  // HTTP 서버 라우트 설정
  server.on("/set_led", HTTP_POST, handleLEDControl);
  server.begin();
}

void loop() {
  server.handleClient();
}

void handleLEDControl() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    
    analogWrite(LED_R, doc["r"].as<int>());
    analogWrite(LED_G, doc["g"].as<int>());
    analogWrite(LED_B, doc["b"].as<int>());
    
    server.send(200, "text/plain", "LED Updated");
  } else {
    server.send(400, "text/plain", "Invalid Request");
  }
}
