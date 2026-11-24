#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>


// định nghĩa hàm 
void setMotor(int speed);
void doc_cam_bien_khoang_cach();
int che_do_tu_hanh();
int tinhtoangoclai(int tin_hieu_tho, int muc_tieu_lai);

// cấu trúc dữ liệu nhận từ tay cầm
typedef struct {
  int16_t x;
  int16_t y;
  bool nut_nhan_tat_tu_hanh;
  bool nut_nhan_kich_hoat_tu_hanh;
} RemotePacket;

// địa chỉ mac từ tay cầm điều khiển
uint8_t controllerMAC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};  // đặt MAC thật của tay cầm ở đây

// Khai báo các chân và cảm biến
#define cau_chu_dong 25 // chân cho servo điều khiển cầu chủ động
#define danh_lai 26 // chân cho servo điều khiển đánh lái
#define TRIG_PIN 27
#define ECHO_PIN 16 // chân cho cảm biến khoảng cách
#define xoay_cam_bien 13 // chân cho servo xoay cảm biến
#define cam_bien_goc_lai 14 // chân cảm biến góc lái

// DC motor (H-bridge) pins (ENA = PWM, IN1/IN2 = direction)
#define motor_en 32
#define motor_in1 33
#define motor_in2 15

// khai báo các trạng thái và biến toàn cục
RemotePacket pkt; // biến toàn cục để lưu dữ liệu nhận được từ tay cầm
bool trang_thai_cau_chu_dong = false; // trạng thái của cầu xe chủ động
int gia_tri_khoang_cach = 0; // giá trị đọc được từ cảm biến khoảng cách
int gia_tri_danh_lai_hien_tai = 0; // giá trị mà góc đánh lái hiện tại đang ở
int muc_tieu_lai_mong_muon = 90; // giá trị mục tiêu lái từ tay cầm điều khiển
int gia_tri_goc_lai = 0; // giá trị đọc được từ cảm biến góc lái
int remoteX = 512; // giá trị joystick ngang nhận từ tay cầm
int remoteY = 512; // giá trị joystick dọc nhận từ tay cầm

Servo servoLai;
Servo servoCamBien;

// setup các giá trị điều kiện điều khiển motor (H-bridge)
const int motorChannel = 0;
const int motorFreq = 5000;
const int motorResolution = 8; // 8-bit duty 0-255

// tạo hàm điều khiển motor qua H-bridge
void setMotor(int speed); // tốc độ từ: -255..255

void onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len != sizeof(RemotePacket)) return;
  memcpy(&pkt, data, sizeof(pkt));
  muc_tieu_lai_mong_muon = map(pkt.x, 0, 1023, 0, 180);
}

void setup() {
  Serial.begin(115200);
  // khởi tạo các chân
  pinMode(cau_chu_dong, OUTPUT);
  pinMode(danh_lai, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(cam_bien_goc_lai, INPUT);

  // tắt cầu chủ động để tránh bật khi mở công tắc
  digitalWrite(cau_chu_dong, LOW);
  trang_thai_cau_chu_dong = false;

  // khai báo đầu ra Motor (H-bridge)
  pinMode(motor_in1, OUTPUT);
  pinMode(motor_in2, OUTPUT);
  pinMode(motor_en, OUTPUT);

  // bật PWM cho motor
  ledcSetup(motorChannel, motorFreq, motorResolution);
  ledcAttachPin(motor_en, motorChannel);
  // tắt motor lúc đầu
  setMotor(0);

  // định nghĩa servo
  servoLai.attach(danh_lai);
  servoLai.write(90);
  servoCamBien.attach(xoay_cam_bien);
  servoCamBien.write(90);

  // Khỏi tạo esp-now
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // tạo callback để nhận dữ liệu từ tay cầm điều khiển
  esp_now_register_recv_cb(onDataRecv);
}

void loop() {
  gia_tri_danh_lai_hien_tai = analogRead(cam_bien_goc_lai);
  // lấy giá trị mục tiêu từ tay cầm điều khiển và tính toán góc lái cần đạt được
  int goc_lai_can_dat = tinhtoangoclai(gia_tri_danh_lai_hien_tai, muc_tieu_lai_mong_muon);
  servoLai.write(goc_lai_can_dat);

  // điều khiển cầu chủ động (nay dùng DC motor qua H-bridge): chỉ bật khi lùi
  // map remoteY -> throttle (-255..255)
  int buom_ga = map(pkt.y, 0, 1023, -255, 255);
  const int muc_tranh_tien_toi_da = 20; // tránh rung
  const int muc_tranh_lui_toi_thieu = -20; // tránh rung
  if (buom_ga < muc_tranh_lui_toi_thieu) { // có điều kiện lùi từ tay cầm
    setMotor(buom_ga); // tiến hành lùi
    trang_thai_cau_chu_dong = true;
  } else {
    if (buom_ga > muc_tranh_tien_toi_da) { // có điều kiện tiến từ tay cầm
      setMotor(buom_ga); // tiến hành tiến
      trang_thai_cau_chu_dong = true; // bật cầu chủ động khi tiến
    } else {
      setMotor(0); // không có điều kiện tiến/lùi, dừng motor
      trang_thai_cau_chu_dong = false; // tắt cầu chủ động khi không lùi
    }
  }

  // nút nhấn kích hoạt chế độ tự hành tránh vật cản
  if (pkt.nut_nhan_kich_hoat_tu_hanh == true) {
    servoLai.write(90); // trả góc lái về thẳng
    che_do_tu_hanh(); // gọi hàm thực hiện chế độ tự hành tránh vật cản
    pkt.nut_nhan_kich_hoat_tu_hanh = false; // reset nút nhấn sau khi thực hiện
    pkt.nut_nhan_tat_tu_hanh = false; // reset nút nhấn tắt chế độ tự hành sau khi thực hiện
  }
}

//hàm tính toán góc lái
int tinhtoangoclai(int tin_hieu_tho, int muc_tieu_lai) {
  int gia_tri_danh_lai_hien_tai = map(tin_hieu_tho, 0, 4095, 0, 180);
  return constrain(muc_tieu_lai, 0, 180);
}

// hàm thực hiện chế đọ tự hành tránh vật cản
int che_do_tu_hanh() {
  while (!pkt.nut_nhan_tat_tu_hanh) {

    //Tiến thẳng cho đến khi gặp vật cản
    doc_cam_bien_khoang_cach();
    while (gia_tri_khoang_cach >= 15) {
      if (pkt.nut_nhan_tat_tu_hanh) { setMotor(0); return 0; }  // kiểm tra nút tắt
      setMotor(150);
      doc_cam_bien_khoang_cach();
      delay(10);
    }

    // Dừng lại khi gặp vật cản
    setMotor(0);
    delay(200);

    // lùi lại trước khi quét
    unsigned long bat_dau_lui = millis();
    while (millis() - bat_dau_lui < 3000) {
      if (pkt.nut_nhan_tat_tu_hanh) { setMotor(0); return 0; }
      setMotor(-150);
      delay(10);
    }
    setMotor(0);
    delay(200);

    //Quét trái và phải bằng servo cảm biến
    servoCamBien.write(120);  // xoay cảm biến sang phải
    delay(200);               // chờ servo đến góc
    doc_cam_bien_khoang_cach();
    int khoang_cach_phai = gia_tri_khoang_cach;

    servoCamBien.write(60);   // xoay cảm biến sang trái
    delay(200);
    doc_cam_bien_khoang_cach();
    int khoang_cach_trai = gia_tri_khoang_cach;

    servoCamBien.write(90);   // trả cảm biến về giữa
    delay(200);

    //Rẽ bánh lái về hướng trống hơn
    if (khoang_cach_phai >= khoang_cach_trai) {
      servoLai.write(120);  // bánh rẽ phải
    } else {
      servoLai.write(60);   // bánh rẽ trái
    }
    delay(500); // chờ bánh lái đến góc

    unsigned long thoi_gian_re = millis();
    while (millis() - thoi_gian_re < 2000) {
      if (pkt.nut_nhan_tat_tu_hanh) { setMotor(0); servoLai.write(90); return 0; }
      setMotor(150); // tiến tới rẽ qua vật cản
      delay(10);
    }

    // trả lái về thẳng sau khi rẽ xong
    servoLai.write(90);
    setMotor(0);
    delay(100);
  }
  setMotor(0);
  servoLai.write(90);
  return 0;
}

// hàm điều khiển motor
void setMotor(int speed) {
  int duty = abs(speed);
  if (duty > 255) duty = 255;
  if (speed > 0) {
    digitalWrite(motor_in1, HIGH);
    digitalWrite(motor_in2, LOW);
    ledcWrite(motorChannel, duty);
  } else if (speed < 0) {
    digitalWrite(motor_in1, LOW);
    digitalWrite(motor_in2, HIGH);
    ledcWrite(motorChannel, duty);
  } else {
    // stop (coast)
    digitalWrite(motor_in1, LOW);
    digitalWrite(motor_in2, LOW);
    ledcWrite(motorChannel, 0);
  }
}

// hàm đọc khoảng cách cảm biến siêu âm
void doc_cam_bien_khoang_cach() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long do_khoang_cach_xung = pulseIn(ECHO_PIN, HIGH, 30000); // timeout 30ms
  gia_tri_khoang_cach = do_khoang_cach_xung * 0.034 / 2; // cm
}