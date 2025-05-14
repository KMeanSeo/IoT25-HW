#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <driver/ledc.h>  // For PWM control

// LED pin definitions
#define LED_RED_PIN    25
#define LED_GREEN_PIN  26
#define LED_BLUE_PIN   27

// PWM settings
#define LED_FREQ        5000
#define LED_RESOLUTION  LEDC_TIMER_8_BIT

// BLE settings
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// PWM channel definitions
#define LED_CHANNEL_R LEDC_CHANNEL_0
#define LED_CHANNEL_G LEDC_CHANNEL_1
#define LED_CHANNEL_B LEDC_CHANNEL_2

BLEServer* pServer;
BLECharacteristic* pLEDCharacteristic;
bool deviceConnected = false;

class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected");
    }
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        pServer->startAdvertising();
    }
};

class CharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String value = pCharacteristic->getValue().c_str();
        if (value.length() > 0) {
            int comma1 = value.indexOf(',');
            int comma2 = value.indexOf(',', comma1+1);
            if(comma1 != -1 && comma2 != -1) {
                int r = value.substring(0, comma1).toInt();
                int g = value.substring(comma1+1, comma2).toInt();
                int b = value.substring(comma2+1).toInt();
                // Value limit (0~255)
                r = constrain(r, 0, 255);
                g = constrain(g, 0, 255);
                b = constrain(b, 0, 255);
                // Set LED color
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LED_CHANNEL_R, r);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LED_CHANNEL_R);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LED_CHANNEL_G, g);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LED_CHANNEL_G);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LED_CHANNEL_B, b);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LED_CHANNEL_B);
                Serial.printf("LED set to R:%d G:%d B:%d\n", r, g, b);
            }
        }
    }
};

void setup() {
    Serial.begin(115200);

    // PWM timer configuration
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LED_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = LED_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    // PWM channel configuration for RED
    ledc_channel_config_t channel_conf = {
        .gpio_num = LED_RED_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LED_CHANNEL_R,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel_conf);

    // PWM channel configuration for GREEN
    channel_conf.gpio_num = LED_GREEN_PIN;
    channel_conf.channel = LED_CHANNEL_G;
    ledc_channel_config(&channel_conf);

    // PWM channel configuration for BLUE
    channel_conf.gpio_num = LED_BLUE_PIN;
    channel_conf.channel = LED_CHANNEL_B;
    ledc_channel_config(&channel_conf);

    // BLE initialization
    BLEDevice::init("LED-Controller");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    pLEDCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    pLEDCharacteristic->setCallbacks(new CharacteristicCallbacks());
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();
    Serial.println("BLE Server Ready!");
}

void loop() {
    delay(2000);
}
