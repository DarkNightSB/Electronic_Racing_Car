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

bool initEspNow(const uint8_t *mac);
void onDataReceived(const uint8_t *mac, const uint8_t *data, int len);
void updateMotor();
void updateSteering();
void updateTurnSignals();
void checkWatchdog();
void logDuLieuNhan();
void logKichChan(int toc_do, uint32_t duty);

const uint8_t mac[] = {0xB0, 0xCB, 0xD8, 0xCD, 0x88, 0x18};

// BTS7960
const uint8_t chan_rpwm = 26;   // quay tới
const uint8_t chan_lpwm = 27;   // quay lùi
const uint8_t chan_r_en = 14;
const uint8_t chan_l_en = 12;
const uint8_t kenh_pwm_rpwm = 0;
const uint8_t kenh_pwm_lpwm = 1;

// Servo lái
const uint8_t chan_servo_lai = 21;
const uint8_t kenh_pwm_servo = 2;

// Xi nhan
const uint8_t chan_xi_nhan_trai = 33;
const uint8_t chan_xi_nhan_phai = 32;

// Cảm biến siêu âm
const uint8_t chan_trig_trai = 22;
const uint8_t chan_echo_trai = 23;
const uint8_t chan_trig_giua_trai = 4;
const uint8_t chan_echo_giua_trai = 5;
const uint8_t chan_trig_giua_phai = 17;
const uint8_t chan_echo_giua_phai = 16;
const uint8_t chan_trig_phai = 18;
const uint8_t chan_echo_phai = 19;

volatile bool packetReceived = false;
volatile uint32_t lastPacketTimeMs = 0;
bool co_bao_watchdog = false;
uint32_t thoi_diem_doi_nhip = 0;
bool trang_thai = false;
DuLieuDieuKhien du_lieu_yeu_cau = {0, 0};

void setup() {
  Serial.begin(9600);

  pinMode(chan_r_en, OUTPUT);
  pinMode(chan_l_en, OUTPUT);
  pinMode(chan_xi_nhan_trai, OUTPUT);
  pinMode(chan_xi_nhan_phai, OUTPUT);
  digitalWrite(chan_r_en, HIGH);
  digitalWrite(chan_l_en, HIGH);
  digitalWrite(chan_xi_nhan_trai, LOW);
  digitalWrite(chan_xi_nhan_phai, LOW);

  if (ledcSetup(kenh_pwm_rpwm, 20000, 8) == 0 || ledcSetup(kenh_pwm_lpwm, 20000, 8) == 0) {
    Serial.println("Cai dat PWM BTS7960 that bai");
    return;
  }
  ledcAttachPin(chan_rpwm, kenh_pwm_rpwm);
  ledcAttachPin(chan_lpwm, kenh_pwm_lpwm);

  if (ledcSetup(kenh_pwm_servo, 50, 16) == 0) {
    Serial.println("Cai dat PWM servo lai that bai");
    return;
  }
  ledcAttachPin(chan_servo_lai, kenh_pwm_servo);

  ledcWrite(kenh_pwm_rpwm, 0);
  ledcWrite(kenh_pwm_lpwm, 0);
  ledcWrite(kenh_pwm_servo, (1600UL * 65535UL) / 20000UL);

  if (initEspNow(mac)) {
    Serial.println("ESP-NOW da khoi tao");
  } else {
    Serial.println("Khoi tao ESP-NOW that bai");
  }
}

void loop() {
  checkWatchdog();
  updateTurnSignals();

  if (packetReceived) {
    packetReceived = false;
    co_bao_watchdog = false;

    logDuLieuNhan();
    updateMotor();
    updateSteering();
  }
}

bool initEspNow(const uint8_t *mac) {
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    return false;
  }

  esp_now_register_recv_cb(onDataReceived);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(mac) && esp_now_add_peer(&peerInfo) != ESP_OK) {
    return false;
  }

  return true;
}

void onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
  (void)mac;

  if (len != sizeof(du_lieu_yeu_cau)) {
    return;
  }

  memcpy(&du_lieu_yeu_cau, data, sizeof(du_lieu_yeu_cau));
  lastPacketTimeMs = millis();
  packetReceived = true;
}

void logDuLieuNhan() {
  Serial.print("Toc do nhan duoc: ");
  Serial.print(du_lieu_yeu_cau.toc_do);
  Serial.print(" | Goc lai nhan duoc: ");
  Serial.println(du_lieu_yeu_cau.goc_lai);
}

void checkWatchdog() {
  if (!co_bao_watchdog && millis() - lastPacketTimeMs > 1000) {
    du_lieu_yeu_cau.toc_do = 0;
    du_lieu_yeu_cau.goc_lai = 0;
    updateMotor();
    updateSteering();
    co_bao_watchdog = true;
  }
}

void updateMotor() {
  int toc_do = constrain(du_lieu_yeu_cau.toc_do, -1000, 1000);
  uint32_t duty = map(abs(toc_do), 0, 1000, 0, 255);

  ledcWrite(kenh_pwm_rpwm, toc_do > 0 ? duty : 0);
  ledcWrite(kenh_pwm_lpwm, toc_do < 0 ? duty : 0);

  logKichChan(toc_do, duty);
}

void logKichChan(int toc_do, uint32_t duty) {
  static uint32_t lan_log_cuoi = 0;
  if (millis() - lan_log_cuoi < 200) {
    return;
  }
  lan_log_cuoi = millis();

  Serial.print("BTS7960 R_EN=");
  Serial.print(digitalRead(chan_r_en));
  Serial.print(" L_EN=");
  Serial.print(digitalRead(chan_l_en));
  Serial.print(" RPWM duty=");
  Serial.print(toc_do > 0 ? duty : 0);
  Serial.print(" LPWM duty=");
  Serial.println(toc_do < 0 ? duty : 0);
}

void updateSteering() {
  int goc_lai = constrain(du_lieu_yeu_cau.goc_lai, -90, 90);
  uint16_t xung_us = map(goc_lai, -90, 90, 1000, 2000);
  uint32_t duty = (xung_us * 65535UL) / 20000UL;

  ledcWrite(kenh_pwm_servo, duty);
}

void updateTurnSignals() {
  int goc_lai = constrain(du_lieu_yeu_cau.goc_lai, -90, 90);

  if (abs(goc_lai) < 10) {
    digitalWrite(chan_xi_nhan_trai, LOW);
    digitalWrite(chan_xi_nhan_phai, LOW);
    trang_thai = false;
    return;
  }

  if (millis() - thoi_diem_doi_nhip >= 300) {
    thoi_diem_doi_nhip = millis();
    trang_thai = !trang_thai;
  }

  digitalWrite(chan_xi_nhan_trai, goc_lai < 0 && trang_thai ? HIGH : LOW);
  digitalWrite(chan_xi_nhan_phai, goc_lai > 0 && trang_thai ? HIGH : LOW);
}
