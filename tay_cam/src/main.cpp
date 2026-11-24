#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

struct DuLieuDieuKhien {
  int16_t toc_do;
  int16_t goc_lai;
};

DuLieuDieuKhien du_lieu_gui_di = {0, 0};

const uint8_t mac_xe[] = {0xF8, 0xB3, 0xB7, 0X33, 0X05, 0X78};

const uint8_t chan_joystick_tien_lui = 33;
const uint8_t chan_joystick_trai_phai = 34;
const int ADC_MIN = 0;
const int ADC_MAX = 4095;
const int TOC_DO_MIN = -1000;
const int TOC_DO_MAX = 1000;
const int GOC_LAI_MIN = -90;
const int GOC_LAI_MAX = 90;

void docJoystickLenXuong();
void docJoystickTraiPhai();
bool guiDuLieuDi();

void setup() {
  Serial.begin(115200);

  pinMode(chan_joystick_tien_lui, INPUT);
  pinMode(chan_joystick_trai_phai, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Khoi tao ESP-NOW that bai");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac_xe, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Them ESP32 xe that bai");
    return;
  }

}

void loop() {
  docJoystickLenXuong();
  docJoystickTraiPhai();
  guiDuLieuDi();
  delay(10);
}

void docJoystickLenXuong() {
  int gia_tri_adc = analogRead(chan_joystick_tien_lui);
  du_lieu_gui_di.toc_do = map(gia_tri_adc, ADC_MIN, ADC_MAX, TOC_DO_MIN, TOC_DO_MAX);
}

void docJoystickTraiPhai() {
  int gia_tri_adc = analogRead(chan_joystick_trai_phai);
  du_lieu_gui_di.goc_lai = map(gia_tri_adc, ADC_MIN, ADC_MAX, GOC_LAI_MIN, GOC_LAI_MAX);
}

bool guiDuLieuDi() {
  esp_err_t result = esp_now_send(mac_xe, (const uint8_t *)&du_lieu_gui_di, sizeof(du_lieu_gui_di));
  return result == ESP_OK;
}
