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
//   pBLEScan->setMaxResults(1);   // 동기화 문제 방지
}

void loop() {
  BLEScanResults *foundDevices = pBLEScan->start(1, false);  // 1초 스캔
  Serial.print("✅ 스캔 완료. 발견된 장치: ");
  Serial.println(foundDevices->getCount());
  pBLEScan->clearResults();
  delay(2000);  // 2초 대기 후 재스캔
}
