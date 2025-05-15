#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ========== 사용자 설정 ========== //
#define TARGET_MAC_ADDRESS "08:a6:f7:a1:46:fa"  // 여기에 타겟 MAC 주소 입력
#define RSSI_SAMPLE_COUNT 7  // 평균을 내기 위한 RSSI 샘플 수
#define RSSI_THRESHOLD 10    // RSSI 변동 임계값 (dB) - 이 값 이상 차이나면 초기화
#define DEFAULT_TX_POWER 0   // 기본 TX Power 값 (서버가 광고 데이터에 TX Power를 포함하지 않을 경우 사용)
// ================================= //

// TX Power 값에 따른 경로 손실 지수 및 기준 RSSI 매핑 테이블
struct TxPowerProfile {
  int8_t txPower;       // dBm 단위의 TX Power
  float pathLossExp;    // 경로 손실 지수 (N)
  float referenceRssi;  // 1m 거리에서의 기준 RSSI
  float alpha;          // 지수 조정 인자
};

// 각 TX Power 레벨에 대한 최적 파라미터 (실험적으로 결정된 값)
const TxPowerProfile TX_POWER_PROFILES[] = {
  {-12, 1.6, -80, 1.1},  // ESP_PWR_LVL_N12
  {-9,  1.7, -77, 1.15}, // ESP_PWR_LVL_N9
  {-6,  1.8, -74, 1.2},  // ESP_PWR_LVL_N6
  {-3,  2.0, -71, 1.25}, // ESP_PWR_LVL_N3
  {0,   2.2, -68, 1.3},  // ESP_PWR_LVL_N0
  {3,   2.4, -65, 1.35}, // ESP_PWR_LVL_P3
  {6,   2.6, -62, 1.4},  // ESP_PWR_LVL_P6
  {9,   2.8, -59, 1.45}  // ESP_PWR_LVL_P9
};

const int TX_POWER_PROFILE_COUNT = sizeof(TX_POWER_PROFILES) / sizeof(TxPowerProfile);

BLEScan* pBLEScan;
BLEAddress* targetAddress;
bool deviceFound = false;

// RSSI 샘플을 저장할 배열과 관련 변수들
int rssiSamples[RSSI_SAMPLE_COUNT];
int rssiSampleIndex = 0;
bool rssiArrayFilled = false;
float lastAverageRssi = 0;  // 마지막 평균 RSSI 값 저장
int8_t lastDetectedTxPower = DEFAULT_TX_POWER;  // 마지막으로 감지된 TX Power 값

class EnhancedAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // MAC 주소로 필터링
    if (advertisedDevice.getAddress().equals(*targetAddress)) {
      deviceFound = true;
      
      // RSSI 값 확인
      int rssi = 0;
      if (advertisedDevice.haveRSSI()) {
        rssi = advertisedDevice.getRSSI();
        
        // 이전 평균 RSSI와 현재 RSSI 차이가 임계값보다 크면 초기화
        if (rssiArrayFilled) {
          float currentAvgRssi = calculateAverageRssi();
          if (abs(rssi - currentAvgRssi) > RSSI_THRESHOLD) {
            Serial.println("⚠️ RSSI 급격한 변동 감지! 샘플 초기화");
            resetRssiSamples();
          }
        }
        
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
      int8_t txPower = DEFAULT_TX_POWER;  // 기본값
      if (advertisedDevice.haveTXPower()) {
        txPower = advertisedDevice.getTXPower();
        lastDetectedTxPower = txPower;  // 감지된 TX Power 저장
      }
      
      // TX Power 프로필 찾기
      TxPowerProfile profile = findTxPowerProfile(txPower);
      
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
        lastAverageRssi = avgRssi;  // 마지막 평균 RSSI 업데이트
        
        // 거리 계산 (TX Power 프로필 사용)
        float distance = calculateDistance(avgRssi, profile);
        
        Serial.print("평균 RSSI: ");
        Serial.print(avgRssi);
        Serial.print(" dBm | 추정 거리: ");
        Serial.print(distance);
        Serial.println(" m");
        Serial.print("사용된 파라미터 - N: ");
        Serial.print(profile.pathLossExp);
        Serial.print(", 기준 RSSI: ");
        Serial.print(profile.referenceRssi);
        Serial.print(", Alpha: ");
        Serial.println(profile.alpha);
        Serial.println("------------------------");
      } else {
        Serial.print("RSSI 샘플 수집 중: ");
        Serial.print(rssiSampleIndex);
        Serial.print("/");
        Serial.println(RSSI_SAMPLE_COUNT);
      }
    }
  }

  // TX Power 값에 가장 가까운 프로필 찾기
  TxPowerProfile findTxPowerProfile(int8_t txPower) {
    // 정확히 일치하는 프로필 찾기
    for (int i = 0; i < TX_POWER_PROFILE_COUNT; i++) {
      if (TX_POWER_PROFILES[i].txPower == txPower) {
        return TX_POWER_PROFILES[i];
      }
    }
    
    // 정확히 일치하는 값이 없으면 가장 가까운 값 찾기
    int closestIndex = 0;
    int minDiff = abs(TX_POWER_PROFILES[0].txPower - txPower);
    
    for (int i = 1; i < TX_POWER_PROFILE_COUNT; i++) {
      int diff = abs(TX_POWER_PROFILES[i].txPower - txPower);
      if (diff < minDiff) {
        minDiff = diff;
        closestIndex = i;
      }
    }
    
    return TX_POWER_PROFILES[closestIndex];
  }

  // RSSI 샘플 초기화 함수
  void resetRssiSamples() {
    for (int i = 0; i < RSSI_SAMPLE_COUNT; i++) {
      rssiSamples[i] = 0;
    }
    rssiSampleIndex = 0;
    rssiArrayFilled = false;
  }

  float calculateAverageRssi() {
    int sum = 0;
    int count = rssiArrayFilled ? RSSI_SAMPLE_COUNT : rssiSampleIndex;
    
    for (int i = 0; i < count; i++) {
      sum += rssiSamples[i];
    }
    
    return (float)sum / count;
  }

  float calculateDistance(float rssi, TxPowerProfile profile) {
    float referenceDistance = 1.0;  // 기준 거리 (m)
    
    // 프로필의 파라미터 사용
    float referenceRssi = profile.referenceRssi;
    float N = profile.pathLossExp;
    float alpha = profile.alpha;
    
    // 지수 계산 및 조정
    float exponent = (referenceRssi - rssi) / (10 * N);
    float exponentAdjusted = (exponent >= 0 ? 1 : -1) * pow(fabs(exponent), alpha);
    
    // 조정된 거리 계산
    float distance = referenceDistance * pow(10, exponentAdjusted);
    
    // 거리 값 범위 제한
    if (distance < 0.1) distance = 0.1;
    if (distance > 20) distance = 20;
    
    return distance;
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 BLE 클라이언트 시작 - 자동 TX Power 감지");
  Serial.print("타겟 MAC 주소: ");
  Serial.println(TARGET_MAC_ADDRESS);
  Serial.print("RSSI 샘플 수: ");
  Serial.println(RSSI_SAMPLE_COUNT);
  Serial.print("RSSI 변동 임계값: ");
  Serial.print(RSSI_THRESHOLD);
  Serial.println(" dB");
  
  Serial.println("\nTX Power 프로필 테이블:");
  for (int i = 0; i < TX_POWER_PROFILE_COUNT; i++) {
    Serial.print("TX Power: ");
    Serial.print(TX_POWER_PROFILES[i].txPower);
    Serial.print(" dBm | N: ");
    Serial.print(TX_POWER_PROFILES[i].pathLossExp);
    Serial.print(" | 기준 RSSI: ");
    Serial.print(TX_POWER_PROFILES[i].referenceRssi);
    Serial.print(" | Alpha: ");
    Serial.println(TX_POWER_PROFILES[i].alpha);
  }
  Serial.println();

  // RSSI 샘플 배열 초기화
  for (int i = 0; i < RSSI_SAMPLE_COUNT; i++) {
    rssiSamples[i] = 0;
  }

  BLEDevice::init("");
  
  // MAC 주소 객체 생성
  targetAddress = new BLEAddress(TARGET_MAC_ADDRESS);
  
  // 클라이언트의 TX Power 설정 (스캔용)
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
  } else {
    Serial.print("현재 감지된 TX Power: ");
    Serial.print(lastDetectedTxPower);
    Serial.println(" dBm");
  }
  
  // 스캔 결과 정리
  pBLEScan->clearResults();
  delay(3000);  // 3초 대기 후 재스캔
}
