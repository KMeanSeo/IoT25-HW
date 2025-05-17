/*********
  Rui Santos
  Adapted by ChatGPT for DHT11
  Original: https://RandomNerdTutorials.com/esp32-ble-server-client/
*********/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>               // ← DHT 라이브러리

// 온도 단위 설정 (주석 처리 시 화씨)
#define temperatureCelsius

// BLE 서버 이름
#define bleServerName "ESP32_BLE_Server"

// ── DHT11 설정 ───────────────────────────────────────────────────────────
#define DHTPIN   21            // DHT11 DATA 핀 → ESP32 GPIO21
#define DHTTYPE  DHT11         // DHT11 종류

DHT dht(DHTPIN, DHTTYPE);
// ──────────────────────────────────────────────────────────────────────────

float temp;
float tempF;
float hum;

// BLE notify 타이머
unsigned long lastTime   = 0;
unsigned long timerDelay = 30000;  // 30초 간격

bool deviceConnected = false;

// 서비스 UUID (변경 없음)
#define SERVICE_UUID "91bad492-b950-4226-aa2b-4ede9fa42f59"

// 온도 characteristic / descriptor
#ifdef temperatureCelsius
  BLECharacteristic bmeTemperatureCelsiusCharacteristics(
    "cba1d466-344c-4be3-ab3f-189f80dd7518",
    BLECharacteristic::PROPERTY_NOTIFY
  );
  BLEDescriptor bmeTemperatureCelsiusDescriptor(
    BLEUUID((uint16_t)0x2902)
  );
#else
  BLECharacteristic bmeTemperatureFahrenheitCharacteristics(
    "f78ebbff-c8b7-4107-93de-889a6a06d408",
    BLECharacteristic::PROPERTY_NOTIFY
  );
  BLEDescriptor bmeTemperatureFahrenheitDescriptor(
    BLEUUID((uint16_t)0x2902)
  );
#endif

// 습도 characteristic / descriptor
BLECharacteristic bmeHumidityCharacteristics(
  "ca73b3ba-39f6-4ab3-91ae-186dc9577d99",
  BLECharacteristic::PROPERTY_NOTIFY
);
BLEDescriptor bmeHumidityDescriptor(BLEUUID((uint16_t)0x2903));

// 클라이언트 연결 콜백
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer)    { deviceConnected = true; }
  void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

void setup() {
  Serial.begin(115200);
  // DHT11 초기화
  dht.begin();

  // BLE 디바이스·서버·서비스 설정 (원본 그대로)
  BLEDevice::init(bleServerName);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *bmeService = pServer->createService(SERVICE_UUID);

  // 온도 characteristic + descriptor
  #ifdef temperatureCelsius
    bmeService->addCharacteristic(&bmeTemperatureCelsiusCharacteristics);
    bmeTemperatureCelsiusDescriptor.setValue("Temperature (°C)");
    bmeTemperatureCelsiusCharacteristics.addDescriptor(&bmeTemperatureCelsiusDescriptor);
  #else
    bmeService->addCharacteristic(&bmeTemperatureFahrenheitCharacteristics);
    bmeTemperatureFahrenheitDescriptor.setValue("Temperature (°F)");
    bmeTemperatureFahrenheitCharacteristics.addDescriptor(&bmeTemperatureFahrenheitDescriptor);
  #endif

  // 습도 characteristic + descriptor
  bmeService->addCharacteristic(&bmeHumidityCharacteristics);
  bmeHumidityDescriptor.setValue("Humidity (%)");
  bmeHumidityCharacteristics.addDescriptor(new BLE2902());

  // 서비스 시작 & 광고 시작
  bmeService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  Serial.println("Waiting for client connection to notify...");
}

void loop() {
  if (!deviceConnected) return;

  if ((millis() - lastTime) > timerDelay) {
    // DHT11에서 온도(°C), 화씨(°F), 습도(%) 읽기
    temp  = dht.readTemperature();
    tempF = dht.readTemperature(true);
    hum   = dht.readHumidity();

    // 온도 notify
    #ifdef temperatureCelsius
      char temperatureCTemp[6];
      dtostrf(temp, 6, 2, temperatureCTemp);
      bmeTemperatureCelsiusCharacteristics.setValue(temperatureCTemp);
      bmeTemperatureCelsiusCharacteristics.notify();
      Serial.print("Temperature Celsius: ");
      Serial.print(temp);
      Serial.print(" °C");
    #else
      char temperatureFTemp[6];
      dtostrf(tempF, 6, 2, temperatureFTemp);
      bmeTemperatureFahrenheitCharacteristics.setValue(temperatureFTemp);
      bmeTemperatureFahrenheitCharacteristics.notify();
      Serial.print("Temperature Fahrenheit: ");
      Serial.print(tempF);
      Serial.print(" °F");
    #endif

    // 습도 notify
    char humidityTemp[6];
    dtostrf(hum, 6, 2, humidityTemp);
    bmeHumidityCharacteristics.setValue(humidityTemp);
    bmeHumidityCharacteristics.notify();
    Serial.print(" - Humidity: ");
    Serial.print(hum);
    Serial.println(" %");

    lastTime = millis();
  }
}
