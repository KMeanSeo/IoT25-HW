#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>

// ========== 사용자 설정 ========== //
#define DEVICE_NAME "ESP32-Server"
#define TX_POWER ESP_PWR_LVL_P9  // +9dBm (최대 전력)
// ================================= //

BLEAdvertising *pAdvertising;
uint32_t advertisingCount = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 BLE 서버 시작 - 광고 모드");

  // BLE 초기화
  BLEDevice::init(DEVICE_NAME);
  
  // TxPower 설정
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, TX_POWER);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, TX_POWER);
  
  // 광고 객체 생성
  pAdvertising = BLEDevice::getAdvertising();
  
  // 광고 설정
  BLEAdvertisementData advData;
  advData.setFlags(0x06); // General discovery, BR/EDR not supported
  advData.setName(DEVICE_NAME);
  
  // TxPower 값을 수동으로 광고 데이터에 포함
  // 0x0A는 TX Power Level AD Type, 0x09는 +9dBm을 나타냄
  uint8_t txPowerData[] = {0x02, 0x0A, 0x09};
  
  // Arduino String으로 변환
  String txPowerString = "";
  for(int i = 0; i < 3; i++) {
    txPowerString += (char)txPowerData[i];
  }
  
  advData.addData(txPowerString);
  
  pAdvertising->setAdvertisementData(advData);
  
  // 스캔 응답 데이터 설정 (선택 사항)
  BLEAdvertisementData scanResponse;
  scanResponse.setName(DEVICE_NAME);
  pAdvertising->setScanResponseData(scanResponse);
  
  // 광고 파라미터 설정
  pAdvertising->setMinPreferred(0x06);  // 0.625ms 단위, 약 3.75ms
  pAdvertising->setMaxPreferred(0x12);  // 0.625ms 단위, 약 11.25ms
  
  // 광고 시작
  pAdvertising->start();
  
  Serial.println("광고 시작됨");
  Serial.print("장치 이름: ");
  Serial.println(DEVICE_NAME);
  Serial.print("TxPower: ");
  Serial.print(TX_POWER);
  Serial.println(" dBm");
}

void loop() {
  // 주기적으로 상태 표시
  Serial.print("광고 중... (");
  Serial.print(advertisingCount++);
  Serial.println(")");
  
  delay(5000); // 5초마다 상태 출력
}


// #include <WiFi.h>
// #include <esp_wifi.h>

// void readMacAddress(){
//   uint8_t baseMac[6];
//   esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
//   if (ret == ESP_OK) {
//     Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
//                   baseMac[0], baseMac[1], baseMac[2],
//                   baseMac[3], baseMac[4], baseMac[5]+=2);
//   } else {
//     Serial.println("Failed to read MAC address");
//   }
// }

// void setup(){
//   Serial.begin(115200);

//   WiFi.mode(WIFI_STA);
//   WiFi.STA.begin();

//   Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
//   readMacAddress();
// }
 
// void loop(){

// }
