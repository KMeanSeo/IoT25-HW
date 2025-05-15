#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ========== 사용자 설정 ========== //
#define TARGET_MAC_ADDRESS "5c:01:3b:33:04:0a"  // 여기에 타겟 MAC 주소 입력
// ================================= //

BLEScan* pBLEScan;
BLEAddress* targetAddress;
bool deviceFound = false;

class EnhancedAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // MAC 주소로 필터링
    if (advertisedDevice.getAddress().equals(*targetAddress)) {
      deviceFound = true;
      
      // RSSI 값 확인
      int rssi = 0;
      if (advertisedDevice.haveRSSI()) {
        rssi = advertisedDevice.getRSSI();
      } else {
        Serial.println("⚠️ RSSI 정보 없음");
      }

      // TX Power 값 확인
      int8_t txPower = -127;  // 기본값
      if (advertisedDevice.haveTXPower()) {
        txPower = advertisedDevice.getTXPower();
      }
      
      // 간단한 결과 출력
      Serial.print("MAC: ");
      Serial.print(advertisedDevice.getAddress().toString().c_str());
      Serial.print(" | RSSI: ");
      Serial.print(rssi);
      Serial.print(" dBm | TxPower: ");
      Serial.print(txPower);
      Serial.println(" dBm");
      
      // 거리 계산 (선택 사항)
      float distance = calculateDistance(rssi, txPower);
      Serial.print("추정 거리: ");
      Serial.print(distance);
      Serial.println(" m");
      Serial.println("------------------------");
    }
  }

  float calculateDistance(int rssi, int txPower) {
    const float N = 2.0;  // 경로 손실 지수(실외 환경)
    return pow(10, (txPower - rssi) / (10 * N));
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 BLE 클라이언트 시작");
  Serial.print("타겟 MAC 주소: ");
  Serial.println(TARGET_MAC_ADDRESS);

  BLEDevice::init("");
  
  // MAC 주소 객체 생성
  targetAddress = new BLEAddress(TARGET_MAC_ADDRESS);
  
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new EnhancedAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(67);
  pBLEScan->setWindow(33);
}

void loop() {
  deviceFound = false;
  Serial.println("타겟 기기 스캔 중...");
  
  // 스캔 실행
  pBLEScan->start(1, false);
  
  // 장치를 찾지 못했을 경우 메시지 출력
  if (!deviceFound) {
    Serial.println("타겟 장치를 찾을 수 없습니다.");
  }
  
  // 스캔 결과 정리
  pBLEScan->clearResults();
  delay(2000);  // 2초 대기 후 재스캔
}
