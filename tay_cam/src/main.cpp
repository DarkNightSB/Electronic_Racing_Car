// mac xe: F8:B3:B7:33:05:78
// mac tay cam: B0:CB:D8:CD:88:18
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

const uint8_t chan_joystick_trai_phai = 33;
const uint8_t chan_joystick_tien_lui = 34;
const int TOC_DO_MIN = -1000;
const int TOC_DO_MAX = 1000;
const int GOC_LAI_MIN = -90;
const int GOC_LAI_MAX = 90;
int adc_giua_tien_lui = 2048;
int adc_giua_trai_phai = 2048;
int adc_tien_lui = 0;
int adc_trai_phai = 0;

void hieuChinhJoystick();
void docJoystickLenXuong();
void docJoystickTraiPhai();
void logTinHieuDieuKhien();
bool guiDuLieuDi();

void setup() {
  Serial.begin(9600);

  pinMode(chan_joystick_tien_lui, INPUT);
  pinMode(chan_joystick_trai_phai, INPUT);
  analogReadResolution(12);
  hieuChinhJoystick();

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
  logTinHieuDieuKhien();
  guiDuLieuDi();
  delay(10);
}

void docJoystickLenXuong() {
  adc_tien_lui = analogRead(chan_joystick_tien_lui);

  if (adc_tien_lui >= adc_giua_tien_lui) {
    du_lieu_gui_di.toc_do = map(adc_tien_lui, adc_giua_tien_lui, 4095, 0, TOC_DO_MAX);
  } else {
    du_lieu_gui_di.toc_do = map(adc_tien_lui, 0, adc_giua_tien_lui, TOC_DO_MIN, 0);
  }
}

void docJoystickTraiPhai() {
  adc_trai_phai = analogRead(chan_joystick_trai_phai);

  if (adc_trai_phai >= adc_giua_trai_phai) {
    du_lieu_gui_di.goc_lai = map(adc_trai_phai, adc_giua_trai_phai, 4095, 0, GOC_LAI_MAX);
  } else {
    du_lieu_gui_di.goc_lai = map(adc_trai_phai, 0, adc_giua_trai_phai, GOC_LAI_MIN, 0);
  }
}

void logTinHieuDieuKhien() {
  Serial.print("ADC tien/lui: ");
  Serial.print(adc_tien_lui);
  Serial.print(" | toc_do: ");
  Serial.print(du_lieu_gui_di.toc_do);
  Serial.print(" || ADC trai/phai: ");
  Serial.print(adc_trai_phai);
  Serial.print(" | goc_lai: ");
  Serial.println(du_lieu_gui_di.goc_lai);
}

void hieuChinhJoystick() {
  long tong_tien_lui = 0;
  long tong_trai_phai = 0;

  for (int i = 0; i < 50; i++) {
    tong_tien_lui += analogRead(chan_joystick_tien_lui);
    tong_trai_phai += analogRead(chan_joystick_trai_phai);
    delay(10);
  }

  adc_giua_tien_lui = tong_tien_lui / 50;
  adc_giua_trai_phai = tong_trai_phai / 50;
}

bool guiDuLieuDi() {
  esp_err_t result = esp_now_send(mac_xe, (const uint8_t *)&du_lieu_gui_di, sizeof(du_lieu_gui_di));
  return result == ESP_OK;
}
