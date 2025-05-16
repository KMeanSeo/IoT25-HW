#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <BLE2902.h>

// ========== 사용자 설정 ========== //
#define DEVICE_NAME "ESP32-Server"
#define TX_POWER ESP_PWR_LVL_P9  // 최대 전송 파워로 설정
// ================================= //

// BLE 서비스 및 특성 UUID 정의
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART 서비스 UUID
#define CHARACTERISTIC_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // RX 특성 UUID
#define LED_SERVICE_UUID    "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define LED_CHAR_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// TX Power 레벨에 따른 dBm 값 매핑
struct TxPowerMapping {
  esp_power_level_t level;
  int8_t dbm_value;
  const char* description;
};

const TxPowerMapping TX_POWER_MAP[] = {
  {ESP_PWR_LVL_N12, -12, "-12 dBm"},
  {ESP_PWR_LVL_N9,  -9,  "-9 dBm"},
  {ESP_PWR_LVL_N6,  -6,  "-6 dBm"},
  {ESP_PWR_LVL_N3,  -3,  "-3 dBm"},
  {ESP_PWR_LVL_N0,   0,  "0 dBm"},
  {ESP_PWR_LVL_P3,   3,  "+3 dBm"},
  {ESP_PWR_LVL_P6,   6,  "+6 dBm"},
  {ESP_PWR_LVL_P9,   9,  "+9 dBm"}
};

const int TX_POWER_MAP_SIZE = sizeof(TX_POWER_MAP) / sizeof(TxPowerMapping);

// 글로벌 변수 선언
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pLedCharacteristic = NULL;
BLEAdvertising *pAdvertising;
uint32_t advertisingCount = 0;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
uint32_t connectionCheckTimer = 0;

// 연결 상태 콜백 클래스
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("클라이언트가 연결되었습니다!");
      // 연결 후에도 광고 계속 - 다른 클라이언트가 연결할 수 있게 함
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("클라이언트 연결이 끊어졌습니다!");
    }
};

class LedCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();  // String 타입으로 변경
      
      if (value.length() > 0) {
        Serial.print("LED 명령 수신: ");
        for (int i = 0; i < value.length(); i++) {
          Serial.print(value[i]);
        }
        Serial.println();
      }
    }
};
// TX Power 레벨에 해당하는 dBm 값 찾기
int8_t getTxPowerDbm(esp_power_level_t level) {
  for (int i = 0; i < TX_POWER_MAP_SIZE; i++) {
    if (TX_POWER_MAP[i].level == level) {
      return TX_POWER_MAP[i].dbm_value;
    }
  }
  return 0; // 기본값
}

// TX Power 레벨에 해당하는 설명 찾기
const char* getTxPowerDescription(esp_power_level_t level) {
  for (int i = 0; i < TX_POWER_MAP_SIZE; i++) {
    if (TX_POWER_MAP[i].level == level) {
      return TX_POWER_MAP[i].description;
    }
  }
  return "Unknown"; // 기본값
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 BLE 서버 시작 - 멀티 연결 지원 (개선된 버전)");

  // BLE 초기화
  BLEDevice::init(DEVICE_NAME);
  
  // TxPower 설정 - 하드웨어 레벨에서 설정
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, TX_POWER);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, TX_POWER);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, TX_POWER);
  
  // 현재 TX Power의 dBm 값 가져오기
  int8_t txPowerDbm = getTxPowerDbm(TX_POWER);
  
  // BLE 서버 생성
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // UART 서비스 생성
  BLEService *pUartService = pServer->createService(SERVICE_UUID);
  
  // UART 특성 생성
  pCharacteristic = pUartService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
  
  // UART 디스크립터 추가
  pCharacteristic->addDescriptor(new BLE2902());
  
  // LED 서비스 생성
  BLEService *pLedService = pServer->createService(LED_SERVICE_UUID);
  
  // LED 특성 생성
  pLedCharacteristic = pLedService->createCharacteristic(
                      LED_CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE
                    );
  
  // LED 특성 콜백 설정
  pLedCharacteristic->setCallbacks(new LedCharacteristicCallbacks());
  
  // 서비스 시작
  pUartService->start();
  pLedService->start();
  
  // 광고 객체 생성
  pAdvertising = BLEDevice::getAdvertising();
  
  // 광고 설정
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->addServiceUUID(LED_SERVICE_UUID);
  
  // 광고 데이터 설정
  BLEAdvertisementData advData;
  advData.setFlags(0x06); // General discovery, BR/EDR not supported
  advData.setName(DEVICE_NAME);
  
  // TxPower 값을 수동으로 광고 데이터에 포함
  uint8_t txPowerData[] = {0x02, 0x0A, (uint8_t)txPowerDbm};
  String txPowerString = "";
  for(int i = 0; i < 3; i++) {
    txPowerString += (char)txPowerData[i];
  }
  advData.addData(txPowerString);
  
  pAdvertising->setAdvertisementData(advData);
  
  // 스캔 응답 데이터 설정
  BLEAdvertisementData scanResponse;
  scanResponse.setName(DEVICE_NAME);
  pAdvertising->setScanResponseData(scanResponse);
  
  // 광고 파라미터 설정 - 연결 안정성을 위해 간격 조정
  pAdvertising->setMinInterval(0x20); // 0.625ms 단위, 약 20ms
  pAdvertising->setMaxInterval(0x40); // 0.625ms 단위, 약 40ms
  
  // 광고 시작
  BLEDevice::startAdvertising();
  
  Serial.println("광고 시작됨 - 멀티 연결 지원");
  Serial.print("장치 이름: ");
  Serial.println(DEVICE_NAME);
  Serial.print("TxPower: ");
  Serial.print(getTxPowerDescription(TX_POWER));
  Serial.print(" (");
  Serial.print(txPowerDbm);
  Serial.println(" dBm)");
  Serial.println("클라이언트 연결 대기 중...");
}

void loop() {
  // 연결된 클라이언트가 있을 때 처리
  if (deviceConnected) {
    // 주기적으로 값 업데이트 (연결 유지를 위한 하트비트)
    if (millis() - connectionCheckTimer > 2000) { // 2초마다 하트비트 전송
      pCharacteristic->setValue((uint8_t*)&value, 4);
      pCharacteristic->notify();
      value++;
      
      Serial.print("연결된 클라이언트에 하트비트 전송: ");
      Serial.println(value);
      
      connectionCheckTimer = millis();
    }
  }
  
  // 연결 해제 처리
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // BLE 스택이 준비할 시간 제공
    pServer->startAdvertising(); // 광고 재시작
    Serial.println("연결이 끊어졌습니다. 광고 재시작...");
    advertisingCount = 0;
    oldDeviceConnected = deviceConnected;
  }
  
  // 새 연결 처리
  if (deviceConnected && !oldDeviceConnected) {
    Serial.println("새 클라이언트가 연결되었습니다!");
    oldDeviceConnected = deviceConnected;
    connectionCheckTimer = millis();
  }
  
  // 광고 상태 표시 (연결이 없을 때만)
  if (!deviceConnected) {
    if (millis() - connectionCheckTimer > 5000) { // 5초마다 메시지 출력
      Serial.print("광고 중... 연결 대기 중 (");
      Serial.print(advertisingCount++);
      Serial.println(")");
      connectionCheckTimer = millis();
    }
  }
  
  delay(100); // CPU 사용량 감소
}
