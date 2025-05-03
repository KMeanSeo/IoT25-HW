// set pin numbers
const int potPin = 34;  // the number of the potentiometer pin
const int ledPin = 5;   // the number of the LED pin

int potValue = 0;

void setup() {
  Serial.begin(9600);
  // initialize the LED pin as an output
  pinMode(ledPin, OUTPUT);
}

void loop() {
  // Reading potentiometer value
  potValue = analogRead(potPin);
  Serial.println(potValue);

  // Map potValue (0-4095 for ESP32 ADC) to delay time in milliseconds
  // Higher potValue -> shorter delay -> faster blinking
  // Lower potValue -> longer delay -> slower blinking
  int delayTime = map(potValue, 0, 4095, 1000, 100); // from 1000ms to 100ms

  digitalWrite(ledPin, HIGH);  // turn the LED on
  delay(delayTime);             // wait for delayTime milliseconds
  digitalWrite(ledPin, LOW);   // turn the LED off
  delay(delayTime);             // wait for delayTime milliseconds
}
