// #include <BLEDevice.h>
// #include <BLEUtils.h>
// #include <BLEScan.h>
// #include <BLEAdvertisedDevice.h>

// // ========== 사용자 설정 ========== //
// #define TARGET_MAC_ADDRESS "08:a6:f7:a1:46:fa"  // 여기에 타겟 MAC 주소 입력
// #define RSSI_SAMPLE_COUNT 7  // 평균을 내기 위한 RSSI 샘플 수
// #define RSSI_THRESHOLD 10    // RSSI 변동 임계값 (dB) - 이 값 이상 차이나면 초기화
// #define DEFAULT_TX_POWER 0   // 기본 TX Power 값 (서버가 광고 데이터에 TX Power를 포함하지 않을 경우 사용)
// // ================================= //

// // TX Power 값에 따른 경로 손실 지수 및 기준 RSSI 매핑 테이블
// struct TxPowerProfile {
//   int8_t txPower;       // dBm 단위의 TX Power
//   float pathLossExp;    // 경로 손실 지수 (N)
//   float referenceRssi;  // 1m 거리에서의 기준 RSSI
//   float alpha;          // 지수 조정 인자
// };

// // 각 TX Power 레벨에 대한 최적 파라미터 (실험적으로 결정된 값)
// const TxPowerProfile TX_POWER_PROFILES[] = {
//   {-12, 1.6, -80, 1.1},  // ESP_PWR_LVL_N12
//   {-9,  1.7, -77, 1.15}, // ESP_PWR_LVL_N9
//   {-6,  1.8, -74, 1.2},  // ESP_PWR_LVL_N6
//   {-3,  2.0, -71, 1.25}, // ESP_PWR_LVL_N3
//   {0,   2.2, -68, 1.4},  // ESP_PWR_LVL_N0
//   {3,   2.4, -76, 1.35}, // ESP_PWR_LVL_P3
//   {6,   2.6, -62, 1.4},  // ESP_PWR_LVL_P6
//   {9,   2.8, -70, 1.45}  // ESP_PWR_LVL_P9
// };

// const int TX_POWER_PROFILE_COUNT = sizeof(TX_POWER_PROFILES) / sizeof(TxPowerProfile);

// BLEScan* pBLEScan;
// BLEAddress* targetAddress;
// bool deviceFound = false;

// // RSSI 샘플을 저장할 배열과 관련 변수들
// int rssiSamples[RSSI_SAMPLE_COUNT];
// int rssiSampleIndex = 0;
// bool rssiArrayFilled = false;
// float lastAverageRssi = 0;  // 마지막 평균 RSSI 값 저장
// int8_t lastDetectedTxPower = DEFAULT_TX_POWER;  // 마지막으로 감지된 TX Power 값

// class EnhancedAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
// private:
//   int consecutiveSpikeCount = 0;  // 연속 튀는 횟수 카운트
//   int spikeToleranceCount = 0;    // 1~2회 튀는 경우 카운트

// public:
//   void onResult(BLEAdvertisedDevice advertisedDevice) {
//     // MAC 주소로 필터링
//     if (advertisedDevice.getAddress().equals(*targetAddress)) {
//       deviceFound = true;
      
//       // RSSI 값 확인
//       int rssi = 0;
//       if (advertisedDevice.haveRSSI()) {
//         rssi = advertisedDevice.getRSSI();
        
//         // 이전 평균 RSSI와 현재 RSSI 차이 계산
//         float currentAvgRssi = calculateAverageRssi();
//         float diff = abs(rssi - currentAvgRssi);
        
//         // 10dB 이상 차이 여부 판단
//         bool isSpike = diff > RSSI_THRESHOLD;
        
//         if (rssiArrayFilled) {
//           if (isSpike) {
//             consecutiveSpikeCount++;
//             spikeToleranceCount++;
//             Serial.print("⚠️ RSSI 급격한 변동 감지! 연속 횟수: ");
//             Serial.println(consecutiveSpikeCount);
            
//             // 연속 5회 이상 튀면 초기화
//             if (consecutiveSpikeCount >= 5) {
//               Serial.println("⚠️ 연속 5회 이상 튀는 현상 발생, 샘플 초기화");
//               resetRssiSamples();
//               consecutiveSpikeCount = 0;
//               spikeToleranceCount = 0;
//             }
//           } else {
//             // 튀지 않는 경우 연속 카운트 초기화
//             if (spikeToleranceCount <= 2) {
//               // 1~2회 튀는 경우는 무시하고 카운트 초기화
//               spikeToleranceCount = 0;
//             }
//             consecutiveSpikeCount = 0;
            
//             // 정상 값은 샘플에 저장
//             rssiSamples[rssiSampleIndex] = rssi;
//             rssiSampleIndex = (rssiSampleIndex + 1) % RSSI_SAMPLE_COUNT;
//             if (rssiSampleIndex == 0) {
//               rssiArrayFilled = true;
//             }
//           }
//         } else {
//           // 초기 샘플 수집 중에는 그냥 저장
//           rssiSamples[rssiSampleIndex] = rssi;
//           rssiSampleIndex = (rssiSampleIndex + 1) % RSSI_SAMPLE_COUNT;
//           if (rssiSampleIndex == 0) {
//             rssiArrayFilled = true;
//           }
//         }
//       } else {
//         Serial.println("⚠️ RSSI 정보 없음");
//         return;
//       }

//       // TX Power 값 확인
//       int8_t txPower = DEFAULT_TX_POWER;  // 기본값
//       if (advertisedDevice.haveTXPower()) {
//         txPower = advertisedDevice.getTXPower();
//         lastDetectedTxPower = txPower;  // 감지된 TX Power 저장
//       }
      
//       // TX Power 프로필 찾기
//       TxPowerProfile profile = findTxPowerProfile(txPower);
      
//       // 간단한 결과 출력
//       Serial.print("MAC: ");
//       Serial.print(advertisedDevice.getAddress().toString().c_str());
//       Serial.print(" | RSSI: ");
//       Serial.print(rssi);
//       Serial.print(" dBm | TxPower: ");
//       Serial.print(txPower);
//       Serial.println(" dBm");
      
//       // 충분한 샘플이 모였을 때만 평균 RSSI 계산 및 거리 추정
//       if (rssiArrayFilled || rssiSampleIndex >= 5) {  // 최소 5개 이상의 샘플이 있을 때
//         // 평균 RSSI 계산 (튀는 값 제외)
//         float avgRssi = calculateAverageRssiExcludeSpikes();
//         lastAverageRssi = avgRssi;  // 마지막 평균 RSSI 업데이트
        
//         // 거리 계산 (TX Power 프로필 사용)
//         float distance = calculateDistance(avgRssi, profile);
        
//         Serial.print("평균 RSSI (튀는 값 제외): ");
//         Serial.print(avgRssi);
//         Serial.print(" dBm | 추정 거리: ");
//         Serial.print(distance);
//         Serial.println(" m");
//         Serial.print("사용된 파라미터 - N: ");
//         Serial.print(profile.pathLossExp);
//         Serial.print(", 기준 RSSI: ");
//         Serial.print(profile.referenceRssi);
//         Serial.print(", Alpha: ");
//         Serial.println(profile.alpha);
//         Serial.println("------------------------");
//       } else {
//         Serial.print("RSSI 샘플 수집 중: ");
//         Serial.print(rssiSampleIndex);
//         Serial.print("/");
//         Serial.println(RSSI_SAMPLE_COUNT);
//       }
//     }
//   }

//   // TX Power 값에 가장 가까운 프로필 찾기
//   TxPowerProfile findTxPowerProfile(int8_t txPower) {
//     // 정확히 일치하는 프로필 찾기
//     for (int i = 0; i < TX_POWER_PROFILE_COUNT; i++) {
//       if (TX_POWER_PROFILES[i].txPower == txPower) {
//         return TX_POWER_PROFILES[i];
//       }
//     }
    
//     // 정확히 일치하는 값이 없으면 가장 가까운 값 찾기
//     int closestIndex = 0;
//     int minDiff = abs(TX_POWER_PROFILES[0].txPower - txPower);
    
//     for (int i = 1; i < TX_POWER_PROFILE_COUNT; i++) {
//       int diff = abs(TX_POWER_PROFILES[i].txPower - txPower);
//       if (diff < minDiff) {
//         minDiff = diff;
//         closestIndex = i;
//       }
//     }
    
//     return TX_POWER_PROFILES[closestIndex];
//   }

//   // RSSI 샘플 초기화 함수
//   void resetRssiSamples() {
//     for (int i = 0; i < RSSI_SAMPLE_COUNT; i++) {
//       rssiSamples[i] = 0;
//     }
//     rssiSampleIndex = 0;
//     rssiArrayFilled = false;
//   }

//   float calculateAverageRssi() {
//     int sum = 0;
//     int count = rssiArrayFilled ? RSSI_SAMPLE_COUNT : rssiSampleIndex;
    
//     for (int i = 0; i < count; i++) {
//       sum += rssiSamples[i];
//     }
    
//     return (count > 0) ? (float)sum / count : 0;
//   }
  
//   // 1~2회 튀는 값 제외하고 평균 계산
//   float calculateAverageRssiExcludeSpikes() {
//     int sum = 0;
//     int count = 0;
//     int spikeCount = 0;
//     int maxSpikesToExclude = 2;
    
//     for (int i = 0; i < (rssiArrayFilled ? RSSI_SAMPLE_COUNT : rssiSampleIndex); i++) {
//       float diff = abs(rssiSamples[i] - lastAverageRssi);
//       if (diff > RSSI_THRESHOLD) {
//         if (spikeCount < maxSpikesToExclude) {
//           spikeCount++;
//           continue; // 튀는 값 제외
//         }
//       }
//       sum += rssiSamples[i];
//       count++;
//     }
    
//     if (count == 0) return lastAverageRssi; // 모두 튀는 값이면 이전 평균 반환
//     return (float)sum / count;
//   }

//   float calculateDistance(float rssi, TxPowerProfile profile) {
//     float referenceDistance = 3.0;  // 기준 거리 (m)
    
//     // 프로필의 파라미터 사용
//     float referenceRssi = profile.referenceRssi;
//     float N = profile.pathLossExp;
//     float alpha = profile.alpha;
    
//     // 지수 계산 및 조정
//     float exponent = (referenceRssi - rssi) / (10 * N);
//     float exponentAdjusted = (exponent >= 0 ? 1 : -1) * pow(fabs(exponent), alpha);
    
//     // 조정된 거리 계산
//     float distance = referenceDistance * pow(10, exponentAdjusted);
    
//     // 거리 값 범위 제한
//     if (distance < 0.1) distance = 0.1;
//     if (distance > 20) distance = 20;
    
//     return distance;
//   }
// };

// void setup() {
//   Serial.begin(115200);
//   Serial.println("\nESP32 BLE 클라이언트 시작 - 개선된 RSSI 필터링");
//   Serial.print("타겟 MAC 주소: ");
//   Serial.println(TARGET_MAC_ADDRESS);
//   Serial.print("RSSI 샘플 수: ");
//   Serial.println(RSSI_SAMPLE_COUNT);
//   Serial.print("RSSI 변동 임계값: ");
//   Serial.print(RSSI_THRESHOLD);
//   Serial.println(" dB");
  
//   Serial.println("\nTX Power 프로필 테이블:");
//   for (int i = 0; i < TX_POWER_PROFILE_COUNT; i++) {
//     Serial.print("TX Power: ");
//     Serial.print(TX_POWER_PROFILES[i].txPower);
//     Serial.print(" dBm | N: ");
//     Serial.print(TX_POWER_PROFILES[i].pathLossExp);
//     Serial.print(" | 기준 RSSI: ");
//     Serial.print(TX_POWER_PROFILES[i].referenceRssi);
//     Serial.print(" | Alpha: ");
//     Serial.println(TX_POWER_PROFILES[i].alpha);
//   }
//   Serial.println();

//   // RSSI 샘플 배열 초기화
//   for (int i = 0; i < RSSI_SAMPLE_COUNT; i++) {
//     rssiSamples[i] = 0;
//   }

//   BLEDevice::init("");
  
//   // MAC 주소 객체 생성
//   targetAddress = new BLEAddress(TARGET_MAC_ADDRESS);
  
//   // 클라이언트의 TX Power 설정 (스캔용)
//   esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  
//   pBLEScan = BLEDevice::getScan();
//   pBLEScan->setAdvertisedDeviceCallbacks(new EnhancedAdvertisedDeviceCallbacks());
//   pBLEScan->setActiveScan(true);
//   pBLEScan->setInterval(67);
//   pBLEScan->setWindow(33);
// }

// void loop() {
//   deviceFound = false;
//   Serial.println("타겟 기기 스캔 중...");
  
//   // 스캔 실행
//   pBLEScan->start(1, false);
  
//   // 장치를 찾지 못했을 경우 메시지 출력
//   if (!deviceFound) {
//     Serial.println("타겟 장치를 찾을 수 없습니다.");
//   } else {
//     Serial.print("현재 감지된 TX Power: ");
//     Serial.print(lastDetectedTxPower);
//     Serial.println(" dBm");
//   }
  
//   // 스캔 결과 정리
//   pBLEScan->clearResults();
//   delay(3000);  // 3초 대기 후 재스캔
// }


#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

// ========== 사용자 설정 ========== //
#define TARGET_MAC_ADDRESS "08:a6:f7:a1:46:fa"  // 거리 측정 대상 MAC 주소
#define RASPBERRY_PI_MAC_ADDRESS "2C:CF:67:31:CE:5B"  // 라즈베리파이 MAC 주소 (실제 주소로 변경 필요)
#define RSSI_SAMPLE_COUNT 7  // 평균을 내기 위한 RSSI 샘플 수
#define RSSI_THRESHOLD 10    // RSSI 변동 임계값 (dB) - 이 값 이상 차이나면 초기화
#define DEFAULT_TX_POWER 0   // 기본 TX Power 값 (서버가 광고 데이터에 TX Power를 포함하지 않을 경우 사용)
// ================================= //

// 라즈베리파이 BLE 서비스 및 특성 UUID
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART 서비스 UUID
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // RX 특성 UUID (라즈베리파이에서 수신)

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
  {0,   2.2, -68, 1.4},  // ESP_PWR_LVL_N0
  {3,   2.4, -76, 1.35}, // ESP_PWR_LVL_P3
  {6,   2.6, -62, 1.4},  // ESP_PWR_LVL_P6
  {9,   2.8, -70, 1.45}  // ESP_PWR_LVL_P9
};

const int TX_POWER_PROFILE_COUNT = sizeof(TX_POWER_PROFILES) / sizeof(TxPowerProfile);

BLEScan* pBLEScan;
BLEAddress* targetAddress;
BLEAddress* raspberryPiAddress;
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
bool deviceFound = false;
bool raspberryPiConnected = false;

// RSSI 샘플을 저장할 배열과 관련 변수들
int rssiSamples[RSSI_SAMPLE_COUNT];
int rssiSampleIndex = 0;
bool rssiArrayFilled = false;
float lastAverageRssi = 0;  // 마지막 평균 RSSI 값 저장
int8_t lastDetectedTxPower = DEFAULT_TX_POWER;  // 마지막으로 감지된 TX Power 값
float lastCalculatedDistance = 0.0;  // 마지막으로 계산된 거리

// 함수 프로토타입 선언 (이 부분이 중요합니다!)
bool connectToRaspberryPi(BLEAdvertisedDevice advertisedDevice);
void sendDistanceToRaspberryPi(float distance);

class EnhancedAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
private:
  int consecutiveSpikeCount = 0;  // 연속 튀는 횟수 카운트
  int spikeToleranceCount = 0;    // 1~2회 튀는 경우 카운트

public:
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // MAC 주소로 필터링
    if (advertisedDevice.getAddress().equals(*targetAddress)) {
      deviceFound = true;
      
      // RSSI 값 확인
      int rssi = 0;
      if (advertisedDevice.haveRSSI()) {
        rssi = advertisedDevice.getRSSI();
        
        // 이전 평균 RSSI와 현재 RSSI 차이 계산
        float currentAvgRssi = calculateAverageRssi();
        float diff = abs(rssi - currentAvgRssi);
        
        // 10dB 이상 차이 여부 판단
        bool isSpike = diff > RSSI_THRESHOLD;
        
        if (rssiArrayFilled) {
          if (isSpike) {
            consecutiveSpikeCount++;
            spikeToleranceCount++;
            Serial.print("⚠️ RSSI 급격한 변동 감지! 연속 횟수: ");
            Serial.println(consecutiveSpikeCount);
            
            // 연속 5회 이상 튀면 초기화
            if (consecutiveSpikeCount >= 5) {
              Serial.println("⚠️ 연속 5회 이상 튀는 현상 발생, 샘플 초기화");
              resetRssiSamples();
              consecutiveSpikeCount = 0;
              spikeToleranceCount = 0;
            }
          } else {
            // 튀지 않는 경우 연속 카운트 초기화
            if (spikeToleranceCount <= 2) {
              // 1~2회 튀는 경우는 무시하고 카운트 초기화
              spikeToleranceCount = 0;
            }
            consecutiveSpikeCount = 0;
            
            // 정상 값은 샘플에 저장
            rssiSamples[rssiSampleIndex] = rssi;
            rssiSampleIndex = (rssiSampleIndex + 1) % RSSI_SAMPLE_COUNT;
            if (rssiSampleIndex == 0) {
              rssiArrayFilled = true;
            }
          }
        } else {
          // 초기 샘플 수집 중에는 그냥 저장
          rssiSamples[rssiSampleIndex] = rssi;
          rssiSampleIndex = (rssiSampleIndex + 1) % RSSI_SAMPLE_COUNT;
          if (rssiSampleIndex == 0) {
            rssiArrayFilled = true;
          }
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
        // 평균 RSSI 계산 (튀는 값 제외)
        float avgRssi = calculateAverageRssiExcludeSpikes();
        lastAverageRssi = avgRssi;  // 마지막 평균 RSSI 업데이트
        
        // 거리 계산 (TX Power 프로필 사용)
        float distance = calculateDistance(avgRssi, profile);
        lastCalculatedDistance = distance;  // 마지막 계산된 거리 저장
        
        Serial.print("평균 RSSI (튀는 값 제외): ");
        Serial.print(avgRssi);
        Serial.print(" dBm | 추정 거리: ");
        Serial.print(distance);
        Serial.println(" m");
        
        // 라즈베리파이로 거리 데이터 전송
        sendDistanceToRaspberryPi(distance);
      } else {
        Serial.print("RSSI 샘플 수집 중: ");
        Serial.print(rssiSampleIndex);
        Serial.print("/");
        Serial.println(RSSI_SAMPLE_COUNT);
      }
    }
    
    // 라즈베리파이 검색 (연결되어 있지 않은 경우)
    if (!raspberryPiConnected && advertisedDevice.getAddress().equals(*raspberryPiAddress)) {
      Serial.println("라즈베리파이 발견! 연결 시도...");
      connectToRaspberryPi(advertisedDevice);
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
    
    return (count > 0) ? (float)sum / count : 0;
  }
  
  // 1~2회 튀는 값 제외하고 평균 계산
  float calculateAverageRssiExcludeSpikes() {
    int sum = 0;
    int count = 0;
    int spikeCount = 0;
    int maxSpikesToExclude = 2;
    
    for (int i = 0; i < (rssiArrayFilled ? RSSI_SAMPLE_COUNT : rssiSampleIndex); i++) {
      float diff = abs(rssiSamples[i] - lastAverageRssi);
      if (diff > RSSI_THRESHOLD) {
        if (spikeCount < maxSpikesToExclude) {
          spikeCount++;
          continue; // 튀는 값 제외
        }
      }
      sum += rssiSamples[i];
      count++;
    }
    
    if (count == 0) return lastAverageRssi; // 모두 튀는 값이면 이전 평균 반환
    return (float)sum / count;
  }

  float calculateDistance(float rssi, TxPowerProfile profile) {
    float referenceDistance = 3.0;  // 기준 거리 (m)
    
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

// 라즈베리파이에 연결하는 함수
bool connectToRaspberryPi(BLEAdvertisedDevice advertisedDevice) {
  if (pClient != nullptr) {
    delete pClient;
  }
  
  pClient = BLEDevice::createClient();
  Serial.println("라즈베리파이에 연결 중...");
  
  // 라즈베리파이에 연결
  if (!pClient->connect(advertisedDevice.getAddress())) {
    Serial.println("연결 실패");
    return false;
  }
  
  Serial.println("라즈베리파이에 연결됨!");
  
  // 서비스 찾기
  BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (pRemoteService == nullptr) {
    Serial.print("서비스를 찾을 수 없음: ");
    Serial.println(SERVICE_UUID);
    pClient->disconnect();
    return false;
  }
  
  // 특성 찾기
  pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("특성을 찾을 수 없음: ");
    Serial.println(CHARACTERISTIC_UUID);
    pClient->disconnect();
    return false;
  }
  
  // 쓰기 권한 확인
  if (!pRemoteCharacteristic->canWrite()) {
    Serial.println("특성에 쓰기 권한이 없음");
    pClient->disconnect();
    return false;
  }
  
  raspberryPiConnected = true;
  Serial.println("라즈베리파이 BLE 서비스에 성공적으로 연결됨");
  return true;
}

// 라즈베리파이로 거리 데이터 전송
void sendDistanceToRaspberryPi(float distance) {
  if (!raspberryPiConnected || pRemoteCharacteristic == nullptr) {
    return;
  }
  
  // 거리 데이터를 문자열로 변환
  char distStr[20];
  sprintf(distStr, "%.2f", distance);
  
  // 데이터 전송
  pRemoteCharacteristic->writeValue(distStr, strlen(distStr));
  Serial.print("라즈베리파이로 거리 데이터 전송: ");
  Serial.println(distStr);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 BLE 클라이언트 시작 - 거리 측정 및 라즈베리파이 전송");
  
  // 타겟 MAC 주소와 라즈베리파이 MAC 주소 설정
  Serial.print("타겟 MAC 주소: ");
  Serial.println(TARGET_MAC_ADDRESS);
  Serial.print("라즈베리파이 MAC 주소: ");
  Serial.println(RASPBERRY_PI_MAC_ADDRESS);
  
  // RSSI 샘플 배열 초기화
  for (int i = 0; i < RSSI_SAMPLE_COUNT; i++) {
    rssiSamples[i] = 0;
  }

  BLEDevice::init("ESP32_Distance_Sensor");
  
  // MAC 주소 객체 생성
  targetAddress = new BLEAddress(TARGET_MAC_ADDRESS);
  raspberryPiAddress = new BLEAddress(RASPBERRY_PI_MAC_ADDRESS);
  
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
  Serial.println("스캔 중...");
  
  // 스캔 실행
  pBLEScan->start(1, false);
  
  // 장치를 찾지 못했을 경우 메시지 출력
  if (!deviceFound) {
    Serial.println("타겟 장치를 찾을 수 없습니다.");
  }
  
  // 라즈베리파이 연결 상태 확인
  if (raspberryPiConnected && pClient != nullptr && !pClient->isConnected()) {
    Serial.println("라즈베리파이 연결이 끊어졌습니다. 재연결 시도...");
    raspberryPiConnected = false;
    pRemoteCharacteristic = nullptr;
  }
  
  // 스캔 결과 정리
  pBLEScan->clearResults();
  delay(3000);  // 3초 대기 후 재스캔
}
