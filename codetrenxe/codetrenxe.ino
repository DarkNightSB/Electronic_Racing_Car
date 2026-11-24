#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>

#define SERVO_PIN 21

// BTS7960
#define RPWM 26
#define LPWM 27
#define R_EN 14
#define L_EN 12

// Ultrasonic Sensors
#define TRIG_LEFT      22
#define ECHO_LEFT      23

#define TRIG_CENTER_L  4
#define ECHO_CENTER_L  5

#define TRIG_CENTER_R  17
#define ECHO_CENTER_R  16

#define TRIG_RIGHT     18
#define ECHO_RIGHT     19

// Xi nhan
#define LED_LEFT  33
#define LED_RIGHT 32

#define SIGNAL_TIMEOUT 300

// Servo
#define STRAIGHT_ANGLE      70
#define TURN_LEFT_ANGLE      0
#define TURN_RIGHT_ANGLE   140

// Né nhẹ sau khi lùi
#define ESCAPE_RIGHT_ANGLE 110
#define ESCAPE_LEFT_ANGLE   30

// Speed
#define AUTO_SPEED  -40
#define BACK_SPEED   40

// Time
#define BACK_TIME 1500
#define FORWARD_ESCAPE_TIME 500

// Sensor
#define OBSTACLE_DIST_CM 7

#define AUTO_START_DELAY 200

Servo myServo;

typedef struct {
  int16_t servo;
  int16_t speed;
  bool autoMode;
} DataPacket;

DataPacket rxData;

volatile bool newPacket = false;

int currentAngle = STRAIGHT_ANGLE;

int distLeft = 200;
int distCenterL = 200;
int distCenterR = 200;
int distRight = 200;

int countLeft = 0;
int countCenterL = 0;
int countCenterR = 0;
int countRight = 0;

unsigned long lastRead = 0;
unsigned long lastPacketTime = 0;
unsigned long lastBlink = 0;
unsigned long autoStartTime = 0;
unsigned long recoverStartTime = 0;

bool ledState = false;
bool lastAutoMode = false;

enum RecoverState {

  RECOVER_NONE,

  RECOVER_BACK_LEFT,
  RECOVER_BACK_RIGHT,

  RECOVER_FORWARD_RIGHT,
  RECOVER_FORWARD_LEFT
};

RecoverState recoverState = RECOVER_NONE;

// =========================
// MOTOR
// =========================
void setMotor(int speed) {

  speed = constrain(speed, -255, 255);

  if (speed > 0) {

    analogWrite(RPWM, speed);
    analogWrite(LPWM, 0);

  }
  else if (speed < 0) {

    analogWrite(RPWM, 0);
    analogWrite(LPWM, -speed);

  }
  else {

    analogWrite(RPWM, 0);
    analogWrite(LPWM, 0);

    digitalWrite(RPWM, LOW);
    digitalWrite(LPWM, LOW);
  }
}

// =========================
// XI NHAN
// =========================
void updateTurnLED(int angle) {

  if (millis() - lastBlink >= 150) {

    lastBlink = millis();

    ledState = !ledState;
  }

  if (angle < 60) {

    digitalWrite(LED_LEFT, ledState);
    digitalWrite(LED_RIGHT, LOW);

  }
  else if (angle > 80) {

    digitalWrite(LED_LEFT, LOW);
    digitalWrite(LED_RIGHT, ledState);

  }
  else {

    digitalWrite(LED_LEFT, LOW);
    digitalWrite(LED_RIGHT, LOW);
  }
}

// =========================
// SERVO
// =========================
void setSteeringAngle(int angle) {

  angle = constrain(angle, 0, 180);

  if (abs(angle - currentAngle) > 1) {

    currentAngle = angle;

    myServo.write(currentAngle);
  }

  updateTurnLED(currentAngle);
}

// =========================
// ULTRASONIC
// =========================
int readDistanceOnce(int trig, int echo) {

  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);

  digitalWrite(trig, LOW);

  long duration = pulseIn(echo, HIGH, 8000);

  if (duration == 0) return 200;

  int d = duration * 0.0343 / 2;

  if (d < 4 || d > 200) return 200;

  return d;
}

int readDistanceCM(int trig, int echo) {

  int d1 = readDistanceOnce(trig, echo);

  delayMicroseconds(250);

  int d2 = readDistanceOnce(trig, echo);

  return min(d1, d2);
}

// =========================
// SENSOR FILTER
// =========================
bool stableBlocked(int distance, int &counter) {

  if (distance <= OBSTACLE_DIST_CM) {

    counter++;

    if (counter > 3) counter = 3;
  }
  else {

    counter--;

    if (counter < 0) counter = 0;
  }

  return counter >= 1;
}

void resetSensorCounters() {

  countLeft = 0;
  countCenterL = 0;
  countCenterR = 0;
  countRight = 0;

  recoverState = RECOVER_NONE;
}

// =========================
// MANUAL
// =========================
void manualControl() {

  int angle = map(rxData.servo,
                  0,
                  4095,
                  TURN_LEFT_ANGLE,
                  TURN_RIGHT_ANGLE);

  angle = constrain(angle,
                    TURN_LEFT_ANGLE,
                    TURN_RIGHT_ANGLE);

  if (abs(rxData.speed) < 15) {
    rxData.speed = 0;
  }

  setSteeringAngle(angle);

  setMotor(rxData.speed);
}

// =========================
// AUTO CONTROL
// =========================
void autoControl() {

  bool leftBlocked =
    stableBlocked(distLeft, countLeft);

  bool centerLBlocked =
    stableBlocked(distCenterL, countCenterL);

  bool centerRBlocked =
    stableBlocked(distCenterR, countCenterR);

  bool rightBlocked =
    stableBlocked(distRight, countRight);

  Serial.print("[AUTO] L=");
  Serial.print(distLeft);

  Serial.print(" CL=");
  Serial.print(distCenterL);

  Serial.print(" CR=");
  Serial.print(distCenterR);

  Serial.print(" R=");
  Serial.println(distRight);

  // ===================================
  // ĐANG NÉ
  // ===================================
  if (recoverState != RECOVER_NONE) {

    // -----------------------------------
    // GẶP TRÁI -> ĐÁNH TRÁI + LÙI
    // -----------------------------------
    if (recoverState == RECOVER_BACK_LEFT) {

      if (millis() - recoverStartTime < BACK_TIME) {

        setSteeringAngle(TURN_LEFT_ANGLE);

        setMotor(BACK_SPEED);

        return;
      }

      // Lùi xong -> về giữa
      setSteeringAngle(STRAIGHT_ANGLE);

      recoverState = RECOVER_FORWARD_RIGHT;

      recoverStartTime = millis();

      return;
    }

    // -----------------------------------
    // ĐÁNH PHẢI 40 ĐỘ + CHẠY TỚI
    // -----------------------------------
    if (recoverState == RECOVER_FORWARD_RIGHT) {

      if (millis() - recoverStartTime
          < FORWARD_ESCAPE_TIME) {

        setSteeringAngle(ESCAPE_RIGHT_ANGLE);

        setMotor(AUTO_SPEED);

        return;
      }

      // XONG -> VỀ GIỮA
      recoverState = RECOVER_NONE;
      setSteeringAngle(STRAIGHT_ANGLE);

      setMotor(AUTO_SPEED);

      return;
    }

    // -----------------------------------
    // GẶP PHẢI -> ĐÁNH PHẢI + LÙI
    // -----------------------------------
    if (recoverState == RECOVER_BACK_RIGHT) {

      if (millis() - recoverStartTime < BACK_TIME) {

        setSteeringAngle(TURN_RIGHT_ANGLE);

        setMotor(BACK_SPEED);

        return;
      }

      // Lùi xong -> về giữa
      setSteeringAngle(STRAIGHT_ANGLE);

      recoverState = RECOVER_FORWARD_LEFT;

      recoverStartTime = millis();

      return;
    }

    // -----------------------------------
    // ĐÁNH TRÁI 40 ĐỘ + CHẠY TỚI
    // -----------------------------------
    if (recoverState == RECOVER_FORWARD_LEFT) {

      if (millis() - recoverStartTime
          < FORWARD_ESCAPE_TIME) {

        setSteeringAngle(ESCAPE_LEFT_ANGLE);

        setMotor(AUTO_SPEED);

        return;
      }

      // XONG -> VỀ GIỮA
      recoverState = RECOVER_NONE;

      setSteeringAngle(STRAIGHT_ANGLE);

      setMotor(AUTO_SPEED);

      return;
    }
  }

  // ===================================
  // GẶP TRÁI
  // ===================================
  if (leftBlocked || centerLBlocked) {

    recoverState = RECOVER_BACK_LEFT;

    recoverStartTime = millis();

    setSteeringAngle(TURN_LEFT_ANGLE);

    setMotor(BACK_SPEED);

    return;
  }

  // ===================================
  // GẶP PHẢI
  // ===================================
  if (rightBlocked || centerRBlocked) {

    recoverState = RECOVER_BACK_RIGHT;

    recoverStartTime = millis();

    setSteeringAngle(TURN_RIGHT_ANGLE);

    setMotor(BACK_SPEED);

    return;
  }

  // ===================================
  // KHÔNG CÓ VẬT CẢN
  // ===================================
  setSteeringAngle(STRAIGHT_ANGLE);

  setMotor(AUTO_SPEED);
}

// =========================
// STOP
// =========================
void stopCar() {

  setMotor(0);

  setSteeringAngle(STRAIGHT_ANGLE);

  digitalWrite(RPWM, LOW);
  digitalWrite(LPWM, LOW);

  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, LOW);
}

// =========================
// ESP NOW RECEIVE
// =========================
void onDataRecv(
  const esp_now_recv_info *info,
  const uint8_t *incomingData,
  int len
) {

  if (len == sizeof(DataPacket)) {

    memcpy(&rxData,
           incomingData,
           sizeof(rxData));

    newPacket = true;

    lastPacketTime = millis();
  }
}

// =========================
// SETUP
// =========================
void setup() {

  Serial.begin(115200);

  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);

  pinMode(R_EN, OUTPUT);
  pinMode(L_EN, OUTPUT);

  digitalWrite(R_EN, HIGH);
  digitalWrite(L_EN, HIGH);

  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);

  pinMode(TRIG_CENTER_L, OUTPUT);
  pinMode(ECHO_CENTER_L, INPUT);

  pinMode(TRIG_CENTER_R, OUTPUT);
  pinMode(ECHO_CENTER_R, INPUT);

  pinMode(TRIG_RIGHT, OUTPUT);
  pinMode(ECHO_RIGHT, INPUT);
  digitalWrite(TRIG_LEFT, LOW);
  digitalWrite(TRIG_CENTER_L, LOW);
  digitalWrite(TRIG_CENTER_R, LOW);
  digitalWrite(TRIG_RIGHT, LOW);

  pinMode(LED_LEFT, OUTPUT);
  pinMode(LED_RIGHT, OUTPUT);

  digitalWrite(LED_LEFT, LOW);
  digitalWrite(LED_RIGHT, LOW);

  myServo.setPeriodHertz(50);

  myServo.attach(SERVO_PIN, 500, 2400);

  myServo.write(STRAIGHT_ANGLE);

  stopCar();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {

    Serial.println("ESP NOW ERROR");

    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  Serial.println("RECEIVER READY");
}

// =========================
// LOOP
// =========================
void loop() {

  if (millis() - lastRead >= 12) {

    lastRead = millis();

    distLeft =
      readDistanceCM(TRIG_LEFT, ECHO_LEFT);

    distCenterL =
      readDistanceCM(TRIG_CENTER_L,
                     ECHO_CENTER_L);

    distCenterR =
      readDistanceCM(TRIG_CENTER_R,
                     ECHO_CENTER_R);

    distRight =
      readDistanceCM(TRIG_RIGHT,
                     ECHO_RIGHT);
  }

  // MẤT TÍN HIỆU
  if (!newPacket ||
      millis() - lastPacketTime > SIGNAL_TIMEOUT) {

    stopCar();

    lastAutoMode = false;

    resetSensorCounters();

    return;
  }

  // VỪA BẬT AUTO
  if (rxData.autoMode &&
      !lastAutoMode) {

    autoStartTime = millis();

    resetSensorCounters();

    setSteeringAngle(STRAIGHT_ANGLE);
  }

  lastAutoMode = rxData.autoMode;

  // AUTO MODE
  if (rxData.autoMode) {

    if (millis() - autoStartTime
        < AUTO_START_DELAY) {

      setSteeringAngle(STRAIGHT_ANGLE);

      setMotor(AUTO_SPEED);
    }
    else {

      autoControl();
    }
  }

  // MANUAL MODE
  else {

    resetSensorCounters();

    manualControl();
  }

  delay(1);
}