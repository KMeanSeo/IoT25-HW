#include <BLEDevice.h>
#include <BLEScan.h>

// Function prototypes
void sendToPi(int rssi, int txPower);

// Raspberry Pi BLE Info
BLEAddress piAddress("2C:CF:67:31:CE:5B"); // Change to actual Raspberry Pi BLE MAC address
BLEClient* piClient;
BLERemoteCharacteristic* dataChar;

// UUID Definitions
#define PI_SERVICE_UUID    BLEUUID("0000ffff-0000-1000-8000-00805f9b34fb")
#define PI_CHAR_UUID       BLEUUID("0000ff01-0000-1000-8000-00805f9b34fb")
#define SERVER_SERVICE_UUID BLEUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b") // ESP32 Server Service UUID

void connectToPi() {
  if (piClient->isConnected()) return;
  if (!piClient->connect(piAddress)) {
    Serial.println("Failed to connect to Raspberry Pi BLE server");
    return;
  }
  BLERemoteService* piService = piClient->getService(PI_SERVICE_UUID);
  if (piService) {
    dataChar = piService->getCharacteristic(PI_CHAR_UUID);
    Serial.println("Successfully connected to Raspberry Pi BLE server");
  } else {
    Serial.println("Failed to find Raspberry Pi BLE service");
  }
}

void sendToPi(int rssi, int txPower) {
  connectToPi();
  if (!dataChar || !dataChar->canWrite()) {
    Serial.println("Cannot write to Raspberry Pi characteristic");
    return;
  }
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%d,%d", rssi, txPower);
  dataChar->writeValue(buffer);
  Serial.printf("Data sent to Raspberry Pi: %s\n", buffer);
}

void scanDevices() {
  BLEScan* scanner = BLEDevice::getScan();
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(99);

  BLEScanResults* results = scanner->start(3); // 3-second scan
  for (int i = 0; i < results->getCount(); i++) {
    BLEAdvertisedDevice device = results->getDevice(i);
    if (device.haveServiceUUID() && device.getServiceUUID().equals(SERVER_SERVICE_UUID)) {
      int rssi = device.getRSSI();
      int txPower = device.getTXPower();
      Serial.printf("[Server Found] RSSI: %d dBm, TX Power: %d dBm\n", rssi, txPower);
      sendToPi(rssi, txPower);
    }
  }
  scanner->clearResults();
}

void setup() {
  Serial.begin(115200);
  BLEDevice::init("BLE-Client");
  piClient = BLEDevice::createClient();
}

void loop() {
  scanDevices();
  delay(5000); // Scan every 5 seconds
}
