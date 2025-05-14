#include <BLEDevice.h>
#include <BLEAdvertising.h>

void setup() {
  BLEDevice::init("Anchor");
  BLEServer *pServer = BLEDevice::createServer();
  
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setTxPower(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9); // 4dBm
  pAdvertising->start();
}

void loop() { delay(1000); }
