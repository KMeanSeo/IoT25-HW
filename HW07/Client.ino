// #include <BLEDevice.h>
// #include <WiFi.h>
// #include <HTTPClient.h>

// // 서버 및 다른 클라이언트 검색
// void scanDevices() {
//   BLEScanResults results = BLEDevice::getScan()->start(5);
  
//   for(int i=0; i<results.getCount(); i++){
//     BLEAdvertisedDevice device = results.getDevice(i);
//     String devName = device.getName().c_str();
    
//     if(devName.startsWith("Anchor") || devName.startsWith("Node")){
//       float distance = calcDistance(device.getRSSI(), device.getTXPower());
//       sendToPi(devName, distance);
//     }
//   }
// }

// // 거리 계산 함수
// float calcDistance(int rssi, int txPower) {
//   const float n = 2.3;
//   return pow(10, (txPower - rssi)/(10 * n));
// }

// // 데이터 전송
// void sendToPi(String target, float dist) {
//   String json = "{\"from\":\"" + String(ESP.getEfuseMac()) 
//               + "\",\"to\":\"" + target 
//               + "\",\"distance\":" + String(dist,2) + "}";
  
//   HTTPClient http;
//   http.begin("http://라즈베리파이_IP:5000/update");
//   http.addHeader("Content-Type", "application/json");
//   http.POST(json);
//   http.end();
// }


#include <WiFi.h>
#include <HTTPClient.h>
#include <BLEDevice.h>

const char* ssid = "your_wifi";
const char* password = "your_password";
const char* serverURL = "http://라즈베리파이_IP:5000/update";

void sendData(float rssi, int txPower) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");
    
    String json = "{";
    json += "\"client_id\":\"" + String(ESP.getEfuseMac()) + "\",";
    json += "\"rssi\":" + String(rssi) + ",";
    json += "\"tx_power\":" + String(txPower);
    json += "}";
    
    http.POST(json);
    http.end();
}

void setup() {
    // WiFi 및 BLE 초기화 코드
}

void loop() {
    // BLE 스캔 및 RSSI 측정 코드
    sendData(measured_rssi, tx_power);
    delay(2000);
}