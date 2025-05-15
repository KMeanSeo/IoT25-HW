#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEClient.h>

// Function prototypes
void sendToPi(int rssi, int txPower);

// Raspberry Pi BLE Info
BLEAddress piAddress("2C:CF:67:31:CE:5B"); // Raspberry Pi BLE MAC 주소
BLEClient* piClient;
BLERemoteCharacteristic* dataChar;
bool connected = false;

// UUID Definitions
#define PI_SERVICE_UUID    "4fafc201-1fb5-459e-8fcc-c5c9c331914b" // 라즈베리파이와 ESP32 서버가 동일한 UUID 사용
#define PI_CHAR_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a8" // 라즈베리파이와 ESP32 서버가 동일한 UUID 사용
#define SERVER_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b" // ESP32 서버 서비스 UUID

// ESP32 서버 MAC 주소 (스캔 결과에서 식별용)
#define SERVER_MAC_ADDRESS "5c:01:3b:33:04:0a" // ESP32 서버의 MAC 주소로 변경

void connectToPi() {
  if (piClient->isConnected()) {
    connected = true;
    return;
  }
  
  Serial.println("Connecting to Raspberry Pi...");
  if (piClient->connect(piAddress)) {
    Serial.println("Connected to Raspberry Pi");
    
    // 서비스 및 특성 검색
    BLERemoteService* piService = piClient->getService(BLEUUID(PI_SERVICE_UUID));
    if (piService) {
      dataChar = piService->getCharacteristic(BLEUUID(PI_CHAR_UUID));
      if (dataChar) {
        Serial.println("Found characteristic on Raspberry Pi");
        connected = true;
      } else {
        Serial.println("Failed to find characteristic on Raspberry Pi");
        piClient->disconnect();
        connected = false;
      }
    } else {
      Serial.println("Failed to find service on Raspberry Pi");
      piClient->disconnect();
      connected = false;
    }
  } else {
    Serial.println("Failed to connect to Raspberry Pi");
    connected = false;
  }
}

void sendToPi(int rssi, int txPower) {
  if (!connected) {
    connectToPi();
  }
  
  if (connected && dataChar && dataChar->canWrite()) {
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%d,%d", rssi, txPower);
    dataChar->writeValue(String(buffer));  // Arduino String 클래스 사용
    Serial.printf("Data sent to Raspberry Pi: %s\n", buffer);
  } else {
    Serial.println("Cannot send data to Raspberry Pi");
    connected = false; // 다음에 다시 연결 시도
  }
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // 서버 MAC 주소로 식별
    if (advertisedDevice.getAddress().toString() == SERVER_MAC_ADDRESS) {
      int rssi = advertisedDevice.getRSSI();
      // TX Power가 없는 경우 기본값 -59 사용 (1m 거리에서의 일반적인 값)
      int txPower = advertisedDevice.haveTXPower() ? advertisedDevice.getTXPower() : -59;
      
      Serial.printf("Server Found - MAC: %s, RSSI: %d dBm, TX Power: %d dBm\n", 
                    advertisedDevice.getAddress().toString().c_str(), rssi, txPower);
      
      // 라즈베리파이에 데이터 전송
      sendToPi(rssi, txPower);
    }
  }
};

void scanDevices() {
  Serial.println("Starting BLE scan...");
  BLEScan* scanner = BLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(99);
  scanner->start(5, false); // 5초 스캔, 계속 실행하지 않음
  Serial.println("Scan completed");
}

void setup() {
  Serial.begin(115200);
  while(!Serial) delay(10); // 시리얼 포트가 준비될 때까지 대기
  
  Serial.println("BLE Client starting...");
  BLEDevice::init("BLE-Client");
  piClient = BLEDevice::createClient();
  
  // 초기 연결 시도
  connectToPi();
}

void loop() {
  // 스캔 실행
  scanDevices();
  
  // 5초 대기 후 다시 스캔
  delay(5000);
}
