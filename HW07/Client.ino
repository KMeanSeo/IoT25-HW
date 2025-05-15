// #include <BLEDevice.h>
// #include <BLEScan.h>
// #include <BLEClient.h>

// // Function prototypes
// void sendToPi(int rssi, int txPower);

// // Raspberry Pi BLE Info
// BLEAddress piAddress("2C:CF:67:31:CE:5B"); // Raspberry Pi BLE MAC 주소
// BLEClient* piClient;
// BLERemoteCharacteristic* dataChar;
// bool connected = false;

// // UUID Definitions
// #define PI_SERVICE_UUID    "4fafc201-1fb5-459e-8fcc-c5c9c331914b" // 라즈베리파이와 ESP32 서버가 동일한 UUID 사용
// #define PI_CHAR_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a8" // 라즈베리파이와 ESP32 서버가 동일한 UUID 사용
// #define SERVER_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b" // ESP32 서버 서비스 UUID

// // ESP32 서버 MAC 주소 (스캔 결과에서 식별용)
// #define SERVER_MAC_ADDRESS "5c:01:3b:33:04:0a" // ESP32 서버의 MAC 주소로 변경

// // 최근 측정된 값 저장
// int lastRssi = 0;
// int lastTxPower = -59; // 기본값
// bool measurementComplete = false; // 측정 완료 여부를 나타내는 플래그

// void connectToPi() {
//   if (piClient->isConnected()) {
//     connected = true;
//     return;
//   }
  
//   Serial.println("Connecting to Raspberry Pi...");
//   if (piClient->connect(piAddress)) {
//     Serial.println("Connected to Raspberry Pi");
    
//     // 서비스 및 특성 검색
//     BLERemoteService* piService = piClient->getService(BLEUUID(PI_SERVICE_UUID));
//     if (piService) {
//       dataChar = piService->getCharacteristic(BLEUUID(PI_CHAR_UUID));
//       if (dataChar) {
//         Serial.println("Found characteristic on Raspberry Pi");
//         connected = true;
//       } else {
//         Serial.println("Failed to find characteristic on Raspberry Pi");
//         piClient->disconnect();
//         connected = false;
//       }
//     } else {
//       Serial.println("Failed to find service on Raspberry Pi");
//       piClient->disconnect();
//       connected = false;
//     }
//   } else {
//     Serial.println("Failed to connect to Raspberry Pi");
//     connected = false;
//   }
// }

// void sendToPi(int rssi, int txPower) {
//   if (!connected) {
//     connectToPi();
//   }
  
//   if (connected && dataChar && dataChar->canWrite()) {
//     char buffer[20];
//     // RSSI와 TX Power만 전송
//     snprintf(buffer, sizeof(buffer), "%d,%d", rssi, txPower);
//     dataChar->writeValue(String(buffer));  // Arduino String 클래스 사용
//     Serial.printf("Data sent to Raspberry Pi: %s\n", buffer);
//     Serial.println("Measurement data successfully sent to Raspberry Pi");
//   } else {
//     Serial.println("Cannot send data to Raspberry Pi");
//     connected = false; // 다음에 다시 연결 시도
//   }
// }

// class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
//   void onResult(BLEAdvertisedDevice advertisedDevice) {
//     // 서버 MAC 주소로 식별
//     if (advertisedDevice.getAddress().toString() == SERVER_MAC_ADDRESS) {
//       // ESP32 서버 발견
//       lastRssi = advertisedDevice.getRSSI();
//       // TX Power가 없는 경우 기본값 -59 사용 (1m 거리에서의 일반적인 값)
//       lastTxPower = advertisedDevice.haveTXPower() ? advertisedDevice.getTXPower() : -59;
      
//       Serial.printf("ESP32 Server Found - MAC: %s, RSSI: %d dBm, TX Power: %d dBm\n", 
//                     advertisedDevice.getAddress().toString().c_str(), lastRssi, lastTxPower);
      
//       // 측정 완료 플래그 설정
//       Serial.println("Measurement completed successfully");
//       measurementComplete = true;
//     }
//   }
// };

// void scanForServer() {
//   Serial.println("Scanning for ESP32 Server...");
//   // 스캔 시작 전에 측정 완료 플래그 초기화
//   measurementComplete = false;
  
//   BLEScan* scanner = BLEDevice::getScan();
//   scanner->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
//   scanner->setActiveScan(true);
//   scanner->setInterval(100);
//   scanner->setWindow(99);
//   scanner->start(3, false); // 3초 스캔
//   Serial.println("Scan completed");
// }

// void setup() {
//   Serial.begin(115200);
//   while(!Serial) delay(10); // 시리얼 포트가 준비될 때까지 대기
  
//   Serial.println("ESP32 BLE Client starting...");
//   // BLE 클라이언트 이름 설정
//   BLEDevice::init("ESP32-RSSI-Client");
//   piClient = BLEDevice::createClient();
  
//   // 초기 연결 시도
//   connectToPi();
// }

// void loop() {
//   // ESP32 서버 스캔
//   scanForServer();
  
//   // 측정이 완료되었으면 라즈베리파이로 데이터 전송
//   if (measurementComplete) {
//     Serial.println("Measurement complete, sending data to Raspberry Pi...");
//     sendToPi(lastRssi, lastTxPower);
//     measurementComplete = false; // 플래그 초기화
//   } else {
//     Serial.println("No measurement data available, skipping transmission");
//   }
  
//   // 2초 대기 후 다시 스캔
//   delay(2000);
// }

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ========== 사용자 설정 ========== //
#define TARGET_DEVICE_NAME "ESP32-Server"
// ================================= //

BLEScan* pBLEScan;

// ▼▼▼ 콜백 클래스 개선 ▼▼▼
class EnhancedAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (!advertisedDevice.haveRSSI()) {
      Serial.println("⚠️ RSSI 정보 수신 실패");
      return;
    }

    if (advertisedDevice.getName() == TARGET_DEVICE_NAME) {
      // 동적 TX Power 파싱
      int8_t txPower = advertisedDevice.haveTXPower() 
                      ? advertisedDevice.getTXPower() 
                      : -127;  // 오류 코드

      int rssi = advertisedDevice.getRSSI();
      
      // 데이터 출력
      Serial.println("\n========================");
      Serial.print("📱 서버 MAC: ");
      Serial.println(advertisedDevice.getAddress().toString().c_str());
      Serial.print("📶 RSSI: ");
      Serial.print(rssi);
      Serial.println(" dBm");
      Serial.print("⚡ TxPower: ");
      Serial.print(txPower);
      Serial.println(" dBm");
      Serial.println("========================");

      // 거리 계산 (선택 사항)
      float distance = calculateDistance(rssi, txPower);
      Serial.print("📏 추정 거리: ");
      Serial.print(distance);
      Serial.println(" m");
    }
  }

  // ▼▼▼ 거리 계산 함수 ▼▼▼
  float calculateDistance(int rssi, int txPower) {
    const float N = 2.0;  // 경로 손실 지수(실외 환경)
    return pow(10, (txPower - rssi) / (10 * N));
  }
};
// ▲▲▲ 개선된 콜백 클래스 ▲▲▲

void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 ESP32 BLE 클라이언트 시작");
  Serial.println("🔍 서버 검색 중...");

  BLEDevice::init("");
  
  // ▼▼▼ 전력 설정 최적화 ▼▼▼
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new EnhancedAdvertisedDeviceCallbacks());
  
  // ▼▼▼ 스캔 파라미터 최적화 ▼▼▼
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(67);    // 41.875ms (BLE 표준)
  pBLEScan->setWindow(33);      // 20.625ms
  pBLEScan->setMaxResults(1);   // 동기화 문제 방지
}

void loop() {
  BLEScanResults foundDevices = pBLEScan->start(1, false);  // 1초 스캔
  Serial.print("✅ 스캔 완료. 발견된 장치: ");
  Serial.println(foundDevices.getCount());
  pBLEScan->clearResults();
  delay(2000);  // 2초 대기 후 재스캔
}
