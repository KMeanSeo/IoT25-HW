#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <BLE2902.h>

// ========== 사용자 설정 ========== //
#define DEVICE_NAME "ESP32-Server"
#define TX_POWER ESP_PWR_LVL_N0  // 여기서 TX Power 레벨 변경 가능
// ================================= //

// BLE 서비스 및 특성 UUID 정의
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

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
BLEAdvertising *pAdvertising;
uint32_t advertisingCount = 0;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

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
  Serial.println("ESP32 BLE 서버 시작 - 멀티 연결 지원");

  // BLE 초기화
  BLEDevice::init(DEVICE_NAME);
  
  // TxPower 설정 - 하드웨어 레벨에서 설정
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, TX_POWER);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, TX_POWER);
  
  // 현재 TX Power의 dBm 값 가져오기
  int8_t txPowerDbm = getTxPowerDbm(TX_POWER);
  
  // BLE 서버 생성
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // BLE 서비스 생성
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // BLE 특성 생성
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );
  
  // BLE 디스크립터 추가
  pCharacteristic->addDescriptor(new BLE2902());
  
  // 서비스 시작
  pService->start();
  
  // 광고 객체 생성
  pAdvertising = BLEDevice::getAdvertising();
  
  // 광고 설정
  pAdvertising->addServiceUUID(SERVICE_UUID);
  
  // 광고 데이터 설정
  BLEAdvertisementData advData;
  advData.setFlags(0x06); // General discovery, BR/EDR not supported
  advData.setName(DEVICE_NAME);
  
  // TxPower 값을 수동으로 광고 데이터에 포함
  // 0x0A는 TX Power Level AD Type
  uint8_t txPowerData[] = {0x02, 0x0A, (uint8_t)txPowerDbm};
  
  // Arduino String으로 변환
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
  
  // 광고 파라미터 설정
  pAdvertising->setMinPreferred(0x06);  // 0.625ms 단위, 약 3.75ms
  pAdvertising->setMaxPreferred(0x12);  // 0.625ms 단위, 약 11.25ms
  
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
  Serial.print("TX Power 데이터: 0x02 0x0A 0x");
  Serial.println((uint8_t)txPowerDbm, HEX);
  Serial.println("클라이언트 연결 대기 중...");
}

void loop() {
  // 연결된 클라이언트가 있을 때 처리
  if (deviceConnected) {
    // 주기적으로 값 업데이트 (선택 사항)
    pCharacteristic->setValue((uint8_t*)&value, 4);
    pCharacteristic->notify();
    value++;
    
    Serial.print("연결된 클라이언트에 알림 전송: ");
    Serial.println(value);
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
  }
  
  // 광고 상태 표시 (연결이 없을 때만)
  if (!deviceConnected) {
    if (advertisingCount % 5 == 0) { // 5초마다 메시지 출력
      Serial.print("광고 중... (");
      Serial.print(advertisingCount / 5);
      Serial.println(")");
    }
    advertisingCount++;
  }
  
  delay(1000);
}
