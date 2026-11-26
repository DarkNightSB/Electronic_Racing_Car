#include <esp_now.h>
#include <WiFi.h>

#define JOY_X 33
#define JOY_Y 34
#define MODE_SW 25

#define MOTOR_DEADZONE 300

uint8_t receiverAddress[] = {0x68, 0x25, 0xDD, 0xEF, 0x14, 0x24};

typedef struct {
  int16_t servo;
  int16_t speed;
  bool autoMode;
} DataPacket;

DataPacket data;

float filtered = 0;
int motorCenter = 0;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  pinMode(MODE_SW, INPUT_PULLUP);
  analogReadResolution(12);

  delay(1000);

  long sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += analogRead(JOY_Y);
    delay(10);
  }

  motorCenter = sum / 50;

  Serial.print("Motor center = ");
  Serial.println(motorCenter);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP NOW init failed");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  int rawX = analogRead(JOY_X);
  int rawY = analogRead(JOY_Y);

  filtered = filtered * 0.7f + rawX * 0.3f;
  data.servo = (int16_t)filtered;

  int speed = 0;

  if (rawY > motorCenter + MOTOR_DEADZONE) {
    speed = map(rawY, motorCenter + MOTOR_DEADZONE, 4095, 0, 255);
  }
  else if (rawY < motorCenter - MOTOR_DEADZONE) {
    speed = map(rawY, 0, motorCenter - MOTOR_DEADZONE, -255, 0);
  }
  else {
    speed = 0;
  }

  if (abs(speed) < 10) speed = 0;

  data.speed = speed;
  data.autoMode = (digitalRead(MODE_SW) == LOW);

  esp_now_send(receiverAddress, (uint8_t *)&data, sizeof(data));

  Serial.print("AUTO: ");
  Serial.print(data.autoMode);
  Serial.print(" | X: ");
  Serial.print(rawX);
  Serial.print(" | Y: ");
  Serial.print(rawY);
  Serial.print(" | Speed: ");
  Serial.println(speed);

  delay(20);
}