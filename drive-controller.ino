/*
  Weed Detection and Removal Droid - Issue #5: Drive System
  Board : ESP32 (Arduino-ESP32 core 2.x or 3.x)     Driver: L298N

  Pin map (ESP32 -> L298N)
    GPIO25 -> IN1   GPIO26 -> IN2   (left motor direction)
    GPIO27 -> IN3   GPIO14 -> IN4   (right motor direction)
    GPIO32 -> ENA   GPIO33 -> ENB   (PWM speed)
    ESP32 GND must be connected to L298N GND.
    Remove the ENA/ENB jumpers on the L298N so the ESP32 PWM controls speed.

  Messages (same as the Node-RED dashboard sends):
    CMD:F  CMD:B  CMD:L  CMD:R  CMD:S      and      SPD:0 ... SPD:255
  They can arrive from the Serial Monitor (115200 baud, newline) or by MQTT
  on topics droid/drive/cmd and droid/drive/speed.

  Truth table (same as the XOD patch)
            IN1 IN2 IN3 IN4   ENA/ENB
    Forward   1   0   1   0    speed
    Backward  0   1   0   1    speed
    Left      0   1   1   0    speed
    Right     1   0   0   1    speed
    Stop      0   0   0   0    0

  Library needed: PubSubClient (install from the Library Manager).
*/

#include <WiFi.h>
#include <PubSubClient.h>

// ---- Network settings (change to match your network) ----
const char* WIFI_SSID   = "YOUR_WIFI_NAME";
const char* WIFI_PASS   = "YOUR_WIFI_PASSWORD";
const char* MQTT_SERVER = "192.168.1.10";   // computer running Node-RED + Mosquitto
const int   MQTT_PORT   = 1883;
const char* TOPIC_CMD   = "droid/drive/cmd";
const char* TOPIC_SPEED = "droid/drive/speed";

// ---- Pins ----
const int PIN_IN1 = 25;
const int PIN_IN2 = 26;
const int PIN_IN3 = 27;
const int PIN_IN4 = 14;
const int PIN_ENA = 32;
const int PIN_ENB = 33;

// ---- PWM: 5 kHz, 8 bit (0..255) ----
const int PWM_FREQ = 5000;
const int PWM_BITS = 8;
const int CH_A = 0;   // used only by core 2.x
const int CH_B = 1;

int  speedValue = 150;   // 0..255, set by SPD:n
char lastCmd    = 'S';   // last direction command

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

void pwmSetup() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_ENA, PWM_FREQ, PWM_BITS);
  ledcAttach(PIN_ENB, PWM_FREQ, PWM_BITS);
#else
  ledcSetup(CH_A, PWM_FREQ, PWM_BITS);
  ledcSetup(CH_B, PWM_FREQ, PWM_BITS);
  ledcAttachPin(PIN_ENA, CH_A);
  ledcAttachPin(PIN_ENB, CH_B);
#endif
}

void pwmWrite(int duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_ENA, duty);
  ledcWrite(PIN_ENB, duty);
#else
  ledcWrite(CH_A, duty);
  ledcWrite(CH_B, duty);
#endif
}

void setDirection(int in1, int in2, int in3, int in4) {
  digitalWrite(PIN_IN1, in1);
  digitalWrite(PIN_IN2, in2);
  digitalWrite(PIN_IN3, in3);
  digitalWrite(PIN_IN4, in4);
}

// Apply the current command and speed to the pins.
void applyOutputs() {
  switch (lastCmd) {
    case 'F': setDirection(1, 0, 1, 0); pwmWrite(speedValue); break;
    case 'B': setDirection(0, 1, 0, 1); pwmWrite(speedValue); break;
    case 'L': setDirection(0, 1, 1, 0); pwmWrite(speedValue); break;
    case 'R': setDirection(1, 0, 0, 1); pwmWrite(speedValue); break;
    default : setDirection(0, 0, 0, 0); pwmWrite(0);          break;  // 'S': ENA = ENB = 0
  }
}

void handleLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line.length() == 5 && line.startsWith("CMD:")) {
    char c = line.charAt(4);
    if (c == 'F' || c == 'B' || c == 'L' || c == 'R' || c == 'S') {
      lastCmd = c;
      applyOutputs();
      Serial.printf("OK command %c, speed %d\n", lastCmd, speedValue);
      return;
    }
  } else if (line.startsWith("SPD:")) {
    String num = line.substring(4);
    bool digitsOnly = num.length() > 0;
    for (unsigned int i = 0; i < num.length(); i++) {
      if (!isDigit(num.charAt(i))) digitsOnly = false;
    }
    if (digitsOnly) {
      speedValue = constrain(num.toInt(), 0, 255);   // clamp to 0..255
      applyOutputs();                                // new speed applies at once
      Serial.printf("OK speed %d\n", speedValue);
      return;
    }
  }
  Serial.println("ERR unknown message: " + line);
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  handleLine(msg);
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " connected" : " failed (Serial control still works)");
}

void connectMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  if (mqtt.connect("esp32-weed-droid")) {
    mqtt.subscribe(TOPIC_CMD);
    mqtt.subscribe(TOPIC_SPEED);
    Serial.println("MQTT connected");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  pwmSetup();
  applyOutputs();                       // starts stopped (lastCmd = 'S')

  connectWifi();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  Serial.println("Drive system ready: CMD:F/B/L/R/S and SPD:0-255");
}

void loop() {
  if (!mqtt.connected()) {
    static unsigned long lastTry = 0;
    if (millis() - lastTry > 3000) { lastTry = millis(); connectMqtt(); }
  }
  mqtt.loop();
  while (Serial.available()) handleLine(Serial.readStringUntil('\n'));
}
