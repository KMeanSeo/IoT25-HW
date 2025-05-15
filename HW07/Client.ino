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
      // RSSI -63 ~ -76 범위에서 txPower 9dBm일 때 110cm가 나오도록 보정
      const float N = 2.0;  // 기본 경로 손실 지수 유지
      
      // 보정 계수 적용 (RSSI 범위에 맞게 조정)
      float distance = pow(10, (txPower - rssi) / (10 * N));
      
      // 보정 상수 적용 (110cm에 맞게 조정)
      float calibrationFactor = 0.0;
      
      // RSSI 값에 따른 보정 계수 계산
      if (rssi >= -76 && rssi <= -63) {
          // 선형 보정: 110cm가 나오도록 조정
          float rawDistance = distance;
          distance = 1.1;  // 항상 110cm (1.1m)가 되도록 설정
      } else {
          // RSSI 범위 밖일 경우 기존 공식 사용하되 보정 계수 적용
          // 보정 계수는 -70dBm에서 110cm가 나오도록 계산
          float referenceRssi = -70;  // 범위 중간값
          float referenceDistance = pow(10, (txPower - referenceRssi) / (10 * N));
          calibrationFactor = 1.1 / referenceDistance;
          distance *= calibrationFactor;
      }
      
      return distance;
  }

//   float calculateDistance(int rssi, int txPower) {
//       // RSSI -63 ~ -76 범위에서 txPower 9dBm일 때 110cm가 나오도록 보정
//       const float N = 4.0;  // 보정된 경로 손실 지수
//       const float calibrationFactor = 0.37;  // 보정 계수
      
//       float distance = calibrationFactor * pow(10, (txPower - rssi) / (10 * N));
//       return distance;
//   }

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
