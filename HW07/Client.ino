#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ========== 사용자 설정 ========== //
#define TARGET_MAC_ADDRESS "5c:01:3b:33:04:0a"  // 여기에 타겟 MAC 주소 입력
#define RSSI_SAMPLE_COUNT 15  // 평균을 내기 위한 RSSI 샘플 수
// ================================= //

BLEScan* pBLEScan;
BLEAddress* targetAddress;
bool deviceFound = false;

// RSSI 샘플을 저장할 배열과 관련 변수들
int rssiSamples[RSSI_SAMPLE_COUNT];
int rssiSampleIndex = 0;
bool rssiArrayFilled = false;

class EnhancedAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // MAC 주소로 필터링
    if (advertisedDevice.getAddress().equals(*targetAddress)) {
      deviceFound = true;
      
      // RSSI 값 확인
      int rssi = 0;
      if (advertisedDevice.haveRSSI()) {
        rssi = advertisedDevice.getRSSI();
        
        // RSSI 값을 배열에 저장
        rssiSamples[rssiSampleIndex] = rssi;
        rssiSampleIndex = (rssiSampleIndex + 1) % RSSI_SAMPLE_COUNT;
        
        // 배열이 완전히 채워졌는지 확인
        if (rssiSampleIndex == 0) {
          rssiArrayFilled = true;
        }
      } else {
        Serial.println("⚠️ RSSI 정보 없음");
        return;
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
      
      // 충분한 샘플이 모였을 때만 평균 RSSI 계산 및 거리 추정
      if (rssiArrayFilled || rssiSampleIndex >= 5) {  // 최소 5개 이상의 샘플이 있을 때
        // 평균 RSSI 계산
        float avgRssi = calculateAverageRssi();
        
        // 거리 계산
        float distance = calculateDistance(avgRssi, txPower);
        
        Serial.print("평균 RSSI: ");
        Serial.print(avgRssi);
        Serial.print(" dBm | 추정 거리: ");
        Serial.print(distance);
        Serial.println(" m");
        Serial.println("------------------------");
      } else {
        Serial.print("RSSI 샘플 수집 중: ");
        Serial.print(rssiSampleIndex);
        Serial.print("/");
        Serial.println(RSSI_SAMPLE_COUNT);
      }
    }
  }

  float calculateAverageRssi() {
    int sum = 0;
    int count = rssiArrayFilled ? RSSI_SAMPLE_COUNT : rssiSampleIndex;
    
    for (int i = 0; i < count; i++) {
      sum += rssiSamples[i];
    }
    
    return (float)sum / count;
  }

  float calculateDistance(float rssi, int txPower) {
    const float N = 2.0;
    float targetDistance = 1.1; // Target distance at reference RSSI
    int refRssiLow = -76;
    int refRssiHigh = -63;
    int referenceRssi = (refRssiLow + refRssiHigh) / 2;

    float rawDistance = pow(10, (txPower - rssi) / (10 * N));
    float referenceDistance = pow(10, (txPower - referenceRssi) / (10 * N));
    float calibrationFactor = targetDistance / referenceDistance;
    float calibratedDistance = rawDistance * calibrationFactor;

    return calibratedDistance;
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 BLE 클라이언트 시작");
  Serial.print("타겟 MAC 주소: ");
  Serial.println(TARGET_MAC_ADDRESS);
  Serial.print("RSSI 샘플 수: ");
  Serial.println(RSSI_SAMPLE_COUNT);

  // RSSI 샘플 배열 초기화
  for (int i = 0; i < RSSI_SAMPLE_COUNT; i++) {
    rssiSamples[i] = 0;
  }

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
