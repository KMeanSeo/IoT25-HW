#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEServer.h>
#include <Arduino.h>

// Service and characteristic UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Target MAC address to scan for (format: AA:BB:CC:DD:EE:FF)
String targetMacAddress = "08:a6:f7:a1:46:fa"; // Replace with your target ESP32 MAC address

// Timing settings
#define SCAN_DURATION 3       // Scan time (seconds)
#define SCAN_INTERVAL 100     // Scan interval (milliseconds)
#define SCAN_WINDOW 99        // Scan window (milliseconds)
#define SERVER_CHECK_INTERVAL 100 // Server task check interval (milliseconds)
#define CONNECTION_TIMEOUT 10000  // Connection timeout (milliseconds)

// RGB LED pins
#define RED_PIN    25
#define GREEN_PIN  26
#define BLUE_PIN   27

// BLE scanner variables
BLEScan* pBLEScan;
bool deviceFound = false;

// BLE server variables
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
unsigned long lastClientActivity = 0;

// Device name variable
String bleDeviceName = "ESP32_RSSI_Server";

// Shared data variables
int rssiValue = 0;
int txPower = 0;
bool newDataAvailable = false;
bool dataSent = true;
SemaphoreHandle_t dataMutex;

// RGB LED variables
int currentRed = 0;
int currentGreen = 0;
int currentBlue = 0;

// Flag to force scan after RGB control
bool forceScan = false;

float calculateDistance(int txPower, int rssi) {
  return pow(10, (txPower - rssi - 69.09) / 31.73);
}

// Function to update BLE device name
void updateBLEDeviceName(const char* newName) {
  esp_ble_gap_set_device_name(newName);
  BLEDevice::stopAdvertising();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.print("BLE device name changed to: ");
  Serial.println(newName);
}

void setColor(int red, int green, int blue) {
  analogWrite(RED_PIN, 255 - red);
  analogWrite(GREEN_PIN, 255 - green);
  analogWrite(BLUE_PIN, 255 - blue);
  currentRed = red;
  currentGreen = green;
  currentBlue = blue;
  Serial.printf("RGB LED set to: R=%d, G=%d, B=%d\n", red, green, blue);
}

// Connection callback class
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    lastClientActivity = millis();
    Serial.println("Client connected!");
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      dataSent = true;
      forceScan = true;
      xSemaphoreGive(dataMutex);
    }
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Client disconnected");
    BLEDevice::startAdvertising();
    Serial.println("Advertising restarted");
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      dataSent = true;
      xSemaphoreGive(dataMutex);
    }
  }
};

// Characteristic callback class to handle write events
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = "";
    if (pCharacteristic->getLength() > 0) {
      for (int i = 0; i < pCharacteristic->getLength(); i++) {
        rxValue += (char)pCharacteristic->getData()[i];
      }
      Serial.print("Received from client: ");
      Serial.println(rxValue);
      if (rxValue.startsWith("RGB:")) {
        String rgbPart = rxValue.substring(4);
        int commaPos1 = rgbPart.indexOf(',');
        int commaPos2 = rgbPart.indexOf(',', commaPos1 + 1);
        if (commaPos1 > 0 && commaPos2 > 0) {
          int r = rgbPart.substring(0, commaPos1).toInt();
          int g = rgbPart.substring(commaPos1 + 1, commaPos2).toInt();
          int b = rgbPart.substring(commaPos2 + 1).toInt();
          setColor(r, g, b);
          String response = "RGB set to: " + String(r) + "," + String(g) + "," + String(b);
          pCharacteristic->setValue(response.c_str());
          pCharacteristic->notify();
          if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            dataSent = true;
            forceScan = true;
            xSemaphoreGive(dataMutex);
          }
        }
      }
    }
  }
};

// Scan result callback class
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String deviceMacAddress = advertisedDevice.getAddress().toString().c_str();
    if (deviceMacAddress.equalsIgnoreCase(targetMacAddress)) {
      deviceFound = true;
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        rssiValue = advertisedDevice.getRSSI();
        if (advertisedDevice.haveTXPower()) {
          txPower = advertisedDevice.getTXPower();
        }
        newDataAvailable = true;
        dataSent = false;
        xSemaphoreGive(dataMutex);
      }
      
      float distance = calculateDistance(txPower, rssiValue);
      Serial.println("----------------------------------------");
      Serial.println("Target ESP32 found!");
      Serial.print("MAC address: ");
      Serial.println(deviceMacAddress);
      Serial.print("RSSI: ");
      Serial.print(rssiValue);
      Serial.println(" dBm");
      if (advertisedDevice.haveTXPower()) {
        Serial.print("TX Power: ");
        Serial.print(txPower);
        Serial.println(" dBm");
      } else {
        Serial.println("TX Power information not included in advertisement packet.");
      }
      Serial.print("Estimated Distance: ");
      Serial.print(distance, 2);
      Serial.println(" meters");
      Serial.println("----------------------------------------");
    }
  }
};

// BLE scan task
void scanTask(void * parameter) {
  Serial.println("Scan task started");
  unsigned long lastScanTime = 0;
  const unsigned long MIN_SCAN_INTERVAL = 500;
  while(1) {
    bool shouldScan = false;
    bool forceImmediateScan = false;
    unsigned long currentTime = millis();
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      shouldScan = dataSent;
      forceImmediateScan = forceScan;
      if (forceImmediateScan) {
        forceScan = false;
      }
      xSemaphoreGive(dataMutex);
    }
    if (shouldScan && (forceImmediateScan || (currentTime - lastScanTime >= MIN_SCAN_INTERVAL))) {
      Serial.println("Scanning...");
      lastScanTime = currentTime;
      BLEScanResults* foundDevices = pBLEScan->start(SCAN_DURATION, false);
      int devicesFound = foundDevices->getCount();
      Serial.print("Devices found: ");
      Serial.println(devicesFound);
      if (!deviceFound) {
        Serial.println("Target ESP32 not found.");
      }
      pBLEScan->clearResults();
      deviceFound = false;
      vTaskDelay(50 / portTICK_PERIOD_MS);
    } else {
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
}

// BLE server task
void serverTask(void * parameter) {
  Serial.println("Server task started");
  while(1) {
    unsigned long currentTime = millis();
    if (deviceConnected && (currentTime - lastClientActivity > CONNECTION_TIMEOUT)) {
      Serial.println("Client connection timeout, resetting connection...");
      pServer->disconnect(0);
      deviceConnected = false;
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        dataSent = true;
        xSemaphoreGive(dataMutex);
      }
      BLEDevice::startAdvertising();
    }
    if (deviceConnected) {
      bool shouldSendData = false;
      String dataToSend = "";
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        if (newDataAvailable && !dataSent) {
          dataToSend = "RSSI:" + String(rssiValue);
          if (txPower != 0) {
            dataToSend += ",TXPower:" + String(txPower);
          }
          dataToSend += ",RGB:" + String(currentRed) + "," + String(currentGreen) + "," + String(currentBlue);
          shouldSendData = true;
          newDataAvailable = false;
          dataSent = true;
        }
        xSemaphoreGive(dataMutex);
      }
      if (shouldSendData) {
        pCharacteristic->setValue(dataToSend.c_str());
        pCharacteristic->notify();
        Serial.println("Data sent to client: " + dataToSend);

        Serial.print("Sent RSSI: ");
        Serial.print(rssiValue);
        Serial.println(" dBm");
        Serial.print("Sent TX Power: ");
        Serial.print(txPower);
        Serial.println(" dBm");

        lastClientActivity = millis();
        vTaskDelay(50 / portTICK_PERIOD_MS);
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
          forceScan = true;
          xSemaphoreGive(dataMutex);
        }
      }
    } else {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        if (newDataAvailable && !dataSent) {
          // Serial.println("No client connection, discarding data and preparing for new scan");
          newDataAvailable = false;
          dataSent = true;
        }
        xSemaphoreGive(dataMutex);
      }
    }
    vTaskDelay(SERVER_CHECK_INTERVAL / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 BLE Scanner and Server with RGB LED control starting...");
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  setColor(0, 0, 0);
  dataMutex = xSemaphoreCreateMutex();
  Serial.print("Setting BLE device name: ");
  Serial.println(bleDeviceName);
  BLEDevice::init(bleDeviceName.c_str());
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pCharacteristic->setValue("ESP32 RSSI Scanner Ready");
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE server started. Advertising...");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(SCAN_INTERVAL);
  pBLEScan->setWindow(SCAN_WINDOW);
  Serial.print("Target MAC address: ");
  Serial.println(targetMacAddress);
  xTaskCreatePinnedToCore(
    scanTask,
    "BLE_Scan",
    8192,
    NULL,
    2,
    NULL,
    0
  );
  xTaskCreatePinnedToCore(
    serverTask,
    "BLE_Server",
    8192,
    NULL,
    1,
    NULL,
    1
  );
}

void loop() {
  delay(1000);
}
