#include <Arduino.h>
#include <WiFi.h>

void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);

  Serial.print("ESP32 MAC cho ESP-NOW: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
}
// mac xe: F8:B3:B7:33:05:78
// mac tay cam: B0:CB:D8:CD:88:18